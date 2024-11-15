#ifndef INC_3DGS_HLSL
#define INC_3DGS_HLSL

#include "../device_shared.hlsl"

#include "3dgs_inc.hlsl"
#include "select.hlsl"
#include "math_constants.hlsl"

void Swap(inout float a, inout float b) {
    float temp = a;
    a = b;
    b = temp;
}
void Swap(inout int a, inout int b) {
    int temp = a;
    a = b;
    b = temp;
}

// Convert NDC2 to screen position
// NDC2: [-1, 1] -> Screen: [0, ScreenDimensions]
float2 NDC2ToScreen(float2 NDC2) {
    return 0.5f * UB.ScreenDimensions * (NDC2 + 1.0f);
}

// Convert screen position to NDC2
// Screen: [0, ScreenDimensions] -> NDC2: [-1, 1]
float2 ScreenToNDC2(float2 Screen) {
    return 2.0f * Screen / UB.ScreenDimensions - 1.0f;
}

float3x4 FetchInstanceTransform (int Index) {
    return g_InstanceTransformBuffer[Index];
}
float3x4 FetchInstanceInverseTransform (int Index) {
    return g_InstanceInvTransformBuffer[Index];
}

float3 GaussianInstance_TransformLocalToWorld (float3 Position, int InstanceIndex) {
    float3x4 Transform = FetchInstanceTransform(InstanceIndex);
    return mul(Transform, float4(Position, 1.0f)).xyz;
}
float3 GaussianInstance_TransformWorldToLocal (float3 Position, int InstanceIndex) {
    float3x4 Transform = FetchInstanceInverseTransform(InstanceIndex);
    return mul(Transform, float4(Position, 1.0f)).xyz;
}

float3 GaussianInstance_TransformLocalToWorld_Vector (float3 Vector, int InstanceIndex) {
    float3x4 Transform = FetchInstanceTransform(InstanceIndex);
    return mul(Transform, float4(Vector, 0.0f)).xyz;
}

float3 GaussianInstance_TransformWorldToLocal_Vector (float3 Vector, int InstanceIndex) {
    float3x4 Transform = FetchInstanceInverseTransform(InstanceIndex);
    return mul(Transform, float4(Vector, 0.0f)).xyz;
}

RayDesc GaussianInstance_TransformWorldToLocal (RayDesc Ray, int InstanceIndex) {
    RayDesc LocalRay = Ray;
    LocalRay.Origin = GaussianInstance_TransformWorldToLocal(Ray.Origin, InstanceIndex);
    float3 LocalDirection = GaussianInstance_TransformWorldToLocal_Vector(Ray.Direction, InstanceIndex);
    float  Length = length(LocalDirection);
    LocalRay.TMin = Ray.TMin * Length;
    LocalRay.TMax = Ray.TMax * Length;
    LocalRay.Direction = normalize(LocalDirection);
    return LocalRay;
}

// 256 instance / 24 (1600 w) gaussians
uint PackGaussianHit (uint InstanceIndex, uint GaussianIndex) {
    return (InstanceIndex << 24) | GaussianIndex;
}
void UnpackGaussianHit (uint Packed, out uint InstanceIndex, out uint GaussianIndex) {
    InstanceIndex = Packed >> 24;
    GaussianIndex = Packed & 0xFFFFFF;
}


Gaussian FetchGaussian(uint Index) {
    float3 Position = g_GaussianPositionBuffer[Index];
    float  Alpha = g_GaussianAlphaBuffer[Index];
    float4 Rotation = g_GaussianRotationBuffer[Index];
    float3 Scale = g_GaussianScaleBuffer[Index];
    Gaussian GaussianData = { Position, Alpha, Rotation, Scale };
    return GaussianData;
}

SHCoefficents3 FetchGaussianSHCoefficients (int Index, int Degree) {
    SHCoefficents3 SHData = (SHCoefficents3) 0;
    SHData.Low.Color = g_GaussianColorBuffer[Index];
    if(Degree >= 1) {
        SHData.Low.SH1[0] = g_GaussianSH1Buffer[Index * 3 + 0];
        SHData.Low.SH1[1] = g_GaussianSH1Buffer[Index * 3 + 1];
        SHData.Low.SH1[2] = g_GaussianSH1Buffer[Index * 3 + 2];
        if(Degree >= 2) {
            SHData.Low.SH2[0] = g_GaussianSH2Buffer[Index * 5 + 0];
            SHData.Low.SH2[1] = g_GaussianSH2Buffer[Index * 5 + 1];
            SHData.Low.SH2[2] = g_GaussianSH2Buffer[Index * 5 + 2];
            SHData.Low.SH2[3] = g_GaussianSH2Buffer[Index * 5 + 3];
            SHData.Low.SH2[4] = g_GaussianSH2Buffer[Index * 5 + 4];
            if(Degree >= 3) {
                SHData.SH3[0] = g_GaussianSH3Buffer[Index * 7 + 0];
                SHData.SH3[1] = g_GaussianSH3Buffer[Index * 7 + 1];
                SHData.SH3[2] = g_GaussianSH3Buffer[Index * 7 + 2];
                SHData.SH3[3] = g_GaussianSH3Buffer[Index * 7 + 3];
                SHData.SH3[4] = g_GaussianSH3Buffer[Index * 7 + 4];
                SHData.SH3[5] = g_GaussianSH3Buffer[Index * 7 + 5];
                SHData.SH3[6] = g_GaussianSH3Buffer[Index * 7 + 6];
            }
        }
    }
    return SHData;
}

// Evaluate a value from a standard 3d normal distribution.
float EvaluateNormalizedGaussian (float3 Position) {
    // The normalization factor is 1 / sqrt((2 * pi)^3)
    const float NormalizationFactor = float(1 / sqrt(8 * M_PI * M_PI * M_PI));
    return NormalizationFactor * exp(-0.5f * dot(Position, Position));
}

float3 QuaternionRotate (float3 Vector, float4 Quaternion) {
    float3 q = Quaternion.xyz;
    float3 t = 2.0f * cross(q, Vector);
    return Vector + Quaternion.w * t + cross(q, t);
}

float4 QuaternionConjugate (float4 Quaternion) {
    return float4(-Quaternion.x, -Quaternion.y, -Quaternion.z, Quaternion.w);
}

float3 QuaternionInverseRotate(float3 Vector, float4 Quaternion) {
    float4 Conjugate = QuaternionConjugate(Quaternion);
    return QuaternionRotate(Vector, Conjugate);
}

// Evaluate the alpha value of a gaussian at a given position
float EvaluateGaussian (Gaussian GaussianData, float3 Position) {
    // x - mu:
    float3 Delta = Position - GaussianData.Position;
    // Transform to gaussian-local space
    float3 RotatedPosition = QuaternionInverseRotate(Delta, GaussianData.Rotation);
    float3 ScaledPosition = float3(
        RotatedPosition.x / GaussianData.Scale.x,
        RotatedPosition.y / GaussianData.Scale.y,
        RotatedPosition.z / GaussianData.Scale.z
    );
    // Evaluate the normalized gaussian
    float Value = EvaluateNormalizedGaussian(ScaledPosition);
    return Value * GaussianData.Alpha;
}

struct SymmetricMatrix {
    float3 Diagonal;
    float3 OffDiagonal;
};

float3x3 ExpandSymmetricMatrix (SymmetricMatrix C) {
    float3x3 MC = float3x3(
        C.Diagonal.x, C.OffDiagonal.x, C.OffDiagonal.y,
        C.OffDiagonal.x, C.Diagonal.y, C.OffDiagonal.z,
        C.OffDiagonal.y, C.OffDiagonal.z, C.Diagonal.z
    );
    return MC;
}

float3x3 EvaluateRotationMatrix (float4 q) {
    float3x3 R = float3x3(
		1.f - 2.f * (q.y * q.y + q.z * q.z), 2.f * (q.x * q.y - q.w * q.z), 2.f * (q.x * q.z + q.w * q.y),
		2.f * (q.x * q.y + q.w * q.z), 1.f - 2.f * (q.x * q.x + q.z * q.z), 2.f * (q.y * q.z - q.w * q.x),
		2.f * (q.x * q.z - q.w * q.y), 2.f * (q.y * q.z + q.w * q.x), 1.f - 2.f * (q.x * q.x + q.y * q.y)
    );
    return R;
}

// Perform frustrum culling
bool IsInFrustrum (
	float3 Position,
	float4x4 View,
	float4x4 Projection,
	out float3 ViewSpacePosition)
{
	// Bring points to screen space
    float4 ViewW = mul(View, float4(Position, 1.0f));
    
    ViewSpacePosition = ViewW.xyz;
	
    float4 Homogeneous = mul(Projection, ViewW);
	float  InvW = 1.f / (Homogeneous.w + 1e-7f);
	float3 Projected = Homogeneous.xyz * InvW;

    float Tolerance = 0.2f;
    float ZNear = 0.2f;

    // Check if the point is inside the frustrum
    if(any(Projected.xy < -1.0f - Tolerance) || any(Projected.xy > 1.0f + Tolerance) 
    // Make sure that z is within 0 and 1
    || Projected.z < ZNear || Projected.z >= 1.f)
    {
        return false;
    }
	return true;
}

float3x3 GetScaleRotationTransform (float3 Scale, float4 Rotation) {
    // Scaling matrix
    float3x3 S = {
        Scale.x, 0, 0,
        0, Scale.y, 0,
        0, 0, Scale.z
    };
    // Rotation matrix
    float3x3 R = EvaluateRotationMatrix(Rotation);
    return mul(S, R);
}

float3x3 GetRotationScaleTransform (float4 Rotation, float3 Scale) {
    // Rotation matrix
    float3x3 R = EvaluateRotationMatrix(Rotation);
    // Scaling matrix
    float3x3 S = {
        Scale.x, 0, 0,
        0, Scale.y, 0,
        0, 0, Scale.z
    };
    return mul(R, S);
}

SymmetricMatrix ComputeCovarianceMatrix (float3 Scale, float4 Rotation) {
    float3x3 M = GetRotationScaleTransform(Rotation, Scale);
    // Covariance matrix
    float3x3 Covariance = mul(M, transpose(M));

    float3 Diagonal = float3(
        Covariance[0][0],
        Covariance[1][1],
        Covariance[2][2]
    );
    float3 OffDiagonal = float3(
        Covariance[0][1],
        Covariance[0][2],
        Covariance[1][2]
    );
    SymmetricMatrix Ret = (SymmetricMatrix)0;
    Ret.Diagonal = Diagonal;
    Ret.OffDiagonal = OffDiagonal;
    return Ret;
}

// Without the constant factor. Used for ray tracing evaluation.
// float NV_EvaluateGaussian (Gaussian GaussianData, float3 Position) {
//     float3 Delta = Position - GaussianData.Position;
//     SymmetricMatrix InvCovariance = ComputeCovarianceMatrix(1.f / GaussianData.Scale, GaussianData.Rotation);
// }

// Project the covariance matrix to 2D
// @return The screen space 2D covariance matrix (m00, m01, m11)
float3 ProjectCovarianceMatrixToScreen(float3 mean, float2 focal, float2 tan_fov, SymmetricMatrix Covariance3D, float4x4 View)
{
    float3 t = mul(View, float4(mean, 1.0)).xyz;
    // -z -> z, become lhs coordinate
    {
        t.z = -t.z;
    }
    const float limx = 0.65f * tan_fov.x;
    const float limy = 0.65f * tan_fov.y;
    const float txtz = t.x / t.z;
    const float tytz = t.y / t.z;
    // Clamp to (fov expanded) frustum
    t.x = clamp(txtz, -limx, limx) * t.z;
    t.y = clamp(tytz, -limy, limy) * t.z;

    float3x3 J = float3x3(
        focal.x / t.z, 0.0f, -(focal.x * t.x) / (t.z * t.z),
        0.0f, focal.y / t.z, -(focal.y * t.y) / (t.z * t.z),
        0.0f, 0.0f, 0.0f);
    // J = transpose(J);

    float3x3 W = float3x3(
        View[0][0], View[0][1], View[0][2],
        View[1][0], View[1][1], View[1][2],
        -View[2][0], -View[2][1], -View[2][2]);
        // View[2][0], View[2][1], View[2][2]);
// #define _REF
#ifdef _REF
    W = transpose(W);
    // float3x3 Mk = mul(J, W);
    float3x3 T = mul(W, J);

    float3x3 C = ExpandSymmetricMatrix(Covariance3D);
    // float3x3 C_2D = mul(mul(Mk, C), transpose(Mk));
    float3x3 C_2D = mul(mul(transpose(T), transpose(C)), T);
#else 
    float3x3 Mk = mul(J, W);
    
    float3x3 C = ExpandSymmetricMatrix(Covariance3D);
    float3x3 C_2D = mul(mul(Mk, C), transpose(Mk));
#endif

    return float3(C_2D[0][0], C_2D[0][1], C_2D[1][1]);
}

float2 UnitVectorToOctahedron(float3 N)
{
	N.xy /= dot( 1, abs(N) );
	if( N.z <= 0 )
	{
		N.xy = ( 1 - abs(N.yx) ) * select(N.xy >= 0, float2(1,1), float2(-1,-1));
	}
	return N.xy;
}

float2 UnitVectorToOctahedron01(float3 N)
{
    return (UnitVectorToOctahedron(N) + 1) * 0.5;
}

float3 OctahedronToUnitVector( float2 Oct )
{
	float3 N = float3( Oct, 1 - dot( 1, abs(Oct) ) );
	float t = max( -N.z, 0 );
	N.xy += select(N.xy >= 0, float2(-t, -t), float2(t, t));
	return normalize(N);
}

float3 Octahedron01ToUnitVector (float2 Oct)
{
    return OctahedronToUnitVector(Oct * 2 - 1);
}

// Clamp to [0, 1)
float saturateDown (float Value) {
    return clamp(Value, 0.0f, 1.f - FLT_EPSILON);
}
float2 saturateDown (float2 Value) {
    return clamp(Value, 0.0f, 1.f - FLT_EPSILON);
}

// Clamp to (0, 1]
float saturateUp (float Value) {
    return clamp(Value, FLT_EPSILON, 1.0f);
}
float2 saturateUp (float2 Value) {
    return clamp(Value, FLT_EPSILON, 1.0f);
}

float UnpackUnorm16 (uint Packed) {
    return Packed / 65535.0f;
}

uint PackUnorm16 (float Unpacked) {
    Unpacked = saturateDown(Unpacked);
    return uint(Unpacked * 65536.0f);
}

float2 UnpackUnorm16x2 (uint Packed) {
    return float2(
        (Packed & 0xFFFF) / 65535.0f,
        (Packed >> 16) / 65535.0f
    );
}

uint PackUnorm16x2 (float2 Unpacked) {
    Unpacked = saturateDown(Unpacked);
    return uint(Unpacked.x * 65536.0f) + (uint(Unpacked.y * 65536.0f) << 16);
}

uint2 PackFp16x4Safe (float4 Unpacked) {
    // Clamp to fp16 range
    Unpacked = clamp(Unpacked, -65504.0f, 65504.0f);
    // Convert to fp16
    uint2 Packed = uint2(
        f32tof16(Unpacked.x) | (f32tof16(Unpacked.y) << 16),
        f32tof16(Unpacked.z) | (f32tof16(Unpacked.w) << 16)
    );
    return Packed;
}

float4 UnpackFp16x4 (uint2 Packed) {
    float4 Unpacked = float4(
        f16tof32(Packed.x & 0xFFFF),
        f16tof32(Packed.x >> 16),
        f16tof32(Packed.y & 0xFFFF),
        f16tof32(Packed.y >> 16)
    );
    return Unpacked;
}

uint2 PackFp16x3Safe (float3 Unpacked) {
    // Clamp to fp16 range
    Unpacked = clamp(Unpacked, -65504.0f, 65504.0f);
    // Convert to fp16
    uint2 Packed = uint2(
        f32tof16(Unpacked.x) | (f32tof16(Unpacked.y) << 16),
        f32tof16(Unpacked.z)
    );
    return Packed;
}

float3 UnpackFp16x3 (uint2 Packed) {
    float3 Unpacked = float3(
        f16tof32(Packed.x & 0xFFFF),
        f16tof32(Packed.x >> 16),
        f16tof32(Packed.y & 0xFFFF)
    );
    return Unpacked;
}

uint2 PackRGBA16 (float4 Unpacked) {
    uint2 RGBPacked = PackFp16x3Safe(Unpacked.xyz);
    return uint2(
        RGBPacked.x,
        RGBPacked.y | (PackUnorm16(Unpacked.w) << 16)
    );
}

float4 UnpackRGBA16 (uint2 Packed) {
    float3 RGBUnpacked = UnpackFp16x3(Packed);
    float AUnpacked = UnpackUnorm16(Packed.y >> 16);
    return float4(RGBUnpacked, AUnpacked);
}

struct RayToTrace {
    float3 Direction;
    float3 Origin;
    float  RayTMin;
    float  RayTMax;
    // Specifies if at least 1 hit has been found
    bool   bHit;
    // Specifies if the hit found is sure to be the closest hit
    bool   bCompleted;
    // The accumulated opacity of the ray traveled so far
    float  AccumulatedOpacity;
};

// Initialize the ray to trace struct
RayToTrace InitRayToTrace () {
    RayToTrace Ray = (RayToTrace)0;
    Ray.RayTMin = 0.0f;
    Ray.RayTMax = FLT_MAX;
    return Ray;
}

// Fetch the ray to trace by its index
RayToTrace FetchRayToTrace (int RayIndex) {
    RayToTrace Ray = (RayToTrace)0;
    Ray.Direction = Octahedron01ToUnitVector(UnpackUnorm16x2(g_RWRayToTraceDirectionBuffer[RayIndex]));
    Ray.Origin    = g_RWRayToTraceOriginBuffer[RayIndex];
    Ray.RayTMax   = g_RWRayToTraceTMaxBuffer[RayIndex];
    Ray.RayTMin   = 0.0f;
    uint Flags = g_RWRayFlagsBuffer[RayIndex];
    Ray.bHit = (Flags & RAY_FLAG_HIT_FOUND_BIT) != 0;
    Ray.bCompleted = (Flags & RAY_FLAG_COMPLETED_BIT) != 0;
    Ray.AccumulatedOpacity = (Flags & RAY_FLAG_OPACITY_MASK) / (RAY_FLAG_OPACITY_MASK + 1.f);
    return Ray;
}

// Write the ray to trace by its index
void WriteRayToTrace (int RayIndex, RayToTrace Ray) {
    g_RWRayToTraceDirectionBuffer[RayIndex] = PackUnorm16x2(UnitVectorToOctahedron01(Ray.Direction));
    float3 NewOrigin = Ray.Origin + Ray.RayTMin * Ray.Direction;
    g_RWRayToTraceOriginBuffer[RayIndex] = NewOrigin;
    g_RWRayToTraceTMaxBuffer[RayIndex] = Ray.RayTMax - Ray.RayTMin;
    uint Flags = 0;
    Flags |= Ray.bHit ? RAY_FLAG_HIT_FOUND_BIT : 0;
    Flags |= Ray.bCompleted ? RAY_FLAG_COMPLETED_BIT : 0;
    Flags |= uint(saturateDown(Ray.AccumulatedOpacity) * (RAY_FLAG_OPACITY_MASK + 1.f));
    g_RWRayFlagsBuffer[RayIndex] = Flags;
}

float4 FetchRayTraceResult (int RayIndex) {
    return UnpackRGBA16(g_RWRayToTraceResultBuffer[RayIndex]);
}

// Compute the ray t to evaluate max respose of a ray-gaussian intersection according to the paper.
// @param Origin The origin of the ray
// @param Direction The direction of the ray
// @param G The gaussian to evaluate
// @return The ray t to evaluate the maximum response of the gaussian along the ray
float EvaluateGaussianResponseRayT (float3 Origin, float3 Direction, Gaussian G, inout float3x3 InvCov) {
    float3x3 InvScaleM = float3x3(
        1.0f / G.Scale.x, 0, 0,
        0, 1.0f / G.Scale.y, 0,
        0, 0, 1.0f / G.Scale.z
    );
    float3x3 R = EvaluateRotationMatrix(G.Rotation);
    InvCov = mul(InvScaleM, transpose(R));
    InvCov = mul(transpose(InvCov), InvCov);
    float3 Temp = mul(InvCov, Direction);
    float  Numerator  = dot(G.Position - Origin, Temp);
    float  Denominator = dot(Direction, Temp);
    return Numerator / max(Denominator, 1e-7f);
}

// Evaluate the gaussian response along the ray
float EvaluateGaussianResponse (float3 Origin, float3 Direction, Gaussian G) {
    float3x3 InvCov;
    float RayT = EvaluateGaussianResponseRayT(Origin, Direction, G, InvCov);
    float3 Position = Origin + RayT * Direction;
    return exp(dot(G.Position - Position, mul(InvCov, Position - G.Position))) * G.Alpha;//EvaluateGaussian(G, Position);
}

// Map color to radiance (using a simple inverse gamma correction)
float3 ColorToRadiance (float3 Color, float Gamma = 2.2f) {
    return pow(Color, Gamma);
}

// Map radiance to color (using a simple gamma correction)
float3 RadianceToColor (float3 Radiance, float Gamma = 2.2f) {
    return pow(Radiance, 1.0f / Gamma);
}

// Map (film space) NDC to a direction in world space
float3 NDC2ToCameraDirectionUnnormalized (float2 NDC2) {
    float3 UnnormalizedDirection = UB.CameraRight * NDC2.x + UB.CameraUp * NDC2.y + UB.CameraDirection;
    return UnnormalizedDirection;
}

float3 NDC2ToCameraDirection (float2 NDC2) {
    return normalize(NDC2ToCameraDirectionUnnormalized(NDC2));
}

// Spawn a camera ray from the screen position
RayToTrace SpawnCameraRay (float2 ScreenPosition) {
    RayToTrace Ray = InitRayToTrace();
    Ray.Origin = UB.CameraPosition;
    float2 NDC2 = ScreenToNDC2(ScreenPosition);
    float3 DirectionUnnormalized = NDC2ToCameraDirectionUnnormalized(NDC2);
    float  Length = length(DirectionUnnormalized);
    Ray.RayTMin = Length * UB.CameraNearPlane;
    Ray.RayTMax = Length * UB.CameraFarPlane;
    Ray.Direction = normalize(DirectionUnnormalized);
    return Ray;
}

float3 DebugColorTable (int Index) {
    const float3 _ColorTable[15] = {
        float3(1, 0, 0),
        float3(0, 1, 0),
        float3(0, 0, 1),
        float3(1, 1, 0),
        float3(1, 0, 1),
        float3(0, 1, 1),
        float3(1, 0.5, 0),
        float3(0, 1, 0.5),
        float3(0.5, 0, 1),
        float3(1, 0, 0.5),
        float3(0, 0.5, 1),
        float3(0.5, 1, 0),
        float3(1, 0.5, 0.5),
        float3(0.5, 1, 0.5),
        float3(0.5, 0.5, 1)
    };
    return _ColorTable[Index % 15];
}

// Gives a heatmap about h in [0, 1]
float3 DebugColorHeatMap (float h) {
    float H = saturate(1.0f - h) * 5.0f;
    float R = saturate(min(H - 1.5f, 4.5f - H));
    float G = saturate(min(H - 0.5f, 3.5f - H));
    float B = saturate(min(H + 0.5f, 2.5f - H));
    return float3(R, G, B);
}

#endif // INC_3DGS_HLSL
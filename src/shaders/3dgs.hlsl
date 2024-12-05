#ifndef INC_3DGS_HLSL
#define INC_3DGS_HLSL

#include "../device_shared.hlsl"

#include "3dgs_inc.hlsl"
#include "select.hlsl"
#include "math_constants.hlsl"

#include "bluenoise.hlsl"

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

float2 NDC2ToUV (float2 NDC2) {
    return float2(0.5f, -0.5f) * NDC2 + 0.5f.xx;
}

float2 UVToNDC2 (float2 UV) {
    return float2(2.f, -2.f) * (UV - 0.5f.xx);
}

// Convert NDC2 to screen position
// NDC2: [-1, 1] -> Screen: [0, ScreenDimensions]
float2 NDC2ToScreen(float2 NDC2) {
    float2 T = float2(NDC2.x, -NDC2.y);
    return 0.5f * UB.ScreenDimensions * (T + 1.0f);
}

float2 NDC2ToFilm (CameraDescription C, float2 NDC2) {
    float2 T = float2(NDC2.x, -NDC2.y);
    return 0.5f * float2(C.FilmDimensions) * (T + 1.0f);
}

// Convert screen position to NDC2
// Screen: [0, ScreenDimensions] -> NDC2: [-1, 1]
float2 ScreenToNDC2(float2 Screen) {
    float2 T = 2.0f * Screen / UB.ScreenDimensions - 1.0f;
    return float2(T.x, -T.y);
}

float2 FilmToNDC2(CameraDescription C, float2 Film) {
    float2 T = 2.f * Film / float2(C.FilmDimensions) - 1.f;
    return float2(T.x, -T.y);
}

float3x4 FetchInstanceTransform (int Index) {
    return g_InstanceTransformBuffer[Index];
}
float3x4 FetchInstanceInverseTransform (int Index) {
    return g_InstanceInvTransformBuffer[Index];
}

float3 TransformPointWithPerspectiveDivide (float4x4 Transform, float3 Point) {
    float4 Homogeneous = mul(Transform, float4(Point, 1.0f));
    return Homogeneous.xyz / Homogeneous.w;
}

float3 TransformPoint (float3x4 Transform, float3 Point) {
    return mul(Transform, float4(Point, 1.0f)).xyz;
}

float3 TransformVector (float3x4 Transform, float3 Vector) {
    return mul(Transform, float4(Vector, 0.0f)).xyz;
}

float3x4 ClipMatrix (float4x4 M) {
    return float3x4(
        M[0][0], M[0][1], M[0][2], M[0][3],
        M[1][0], M[1][1], M[1][2], M[1][3],
        M[2][0], M[2][1], M[2][2], M[2][3]
    );
}

float3 TransformVector (float3x3 Transform, float3 Vector) {
    return mul(Transform, Vector);
}

float3 GaussianInstance_TransformLocalToWorld (float3 Position, int InstanceIndex) {
    float3x4 Transform = FetchInstanceTransform(InstanceIndex);
    return TransformPoint(Transform, Position);
}
float3 GaussianInstance_TransformWorldToLocal (float3 Position, int InstanceIndex) {
    float3x4 Transform = FetchInstanceInverseTransform(InstanceIndex);
    return TransformPoint(Transform, Position);
}

float3 GaussianInstance_TransformLocalToWorld_Normal (float3 Normal, int InstanceIndex) {
    float3x3 Transform = g_InstanceNormalTransformBuffer[InstanceIndex];
    return TransformVector(Transform, Normal);
}

float3 GaussianInstance_TransformLocalToWorld_Vector (float3 Vector, int InstanceIndex) {
    float3x4 Transform = FetchInstanceTransform(InstanceIndex);
    return mul(Transform, float4(Vector, 0.0f)).xyz;
}

float3 GaussianInstance_TransformWorldToLocal_Vector (float3 Vector, int InstanceIndex) {
    float3x4 Transform = FetchInstanceInverseTransform(InstanceIndex);
    return mul(Transform, float4(Vector, 0.0f)).xyz;
}

RayDesc GaussianInstance_TransformWorldToLocal (RayDesc Ray, int InstanceIndex, out float RayScaler) {
    RayDesc LocalRay = Ray;
    LocalRay.Origin = GaussianInstance_TransformWorldToLocal(Ray.Origin, InstanceIndex);
    float3 LocalDirection = GaussianInstance_TransformWorldToLocal_Vector(Ray.Direction, InstanceIndex);
    RayScaler = length(LocalDirection);
    LocalRay.TMin = Ray.TMin * RayScaler;
    LocalRay.TMax = Ray.TMax * RayScaler;
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


Gaussian FetchGaussian (uint Index) {
    float3 Position = g_GaussianPositionBuffer[Index];
    float  Alpha    = g_GaussianAlphaBuffer[Index];
    float4 Rotation = g_GaussianRotationBuffer[Index];
    float3 Scale    = g_GaussianScaleBuffer[Index];
    Gaussian GaussianData = { Position, Alpha, Rotation, Scale};
    return GaussianData;
}

GaussianPBR FetchGaussianPBR (uint Index) {
    float3 Normal    = g_GaussianNormalBuffer[Index];
    float3 Albedo    = g_GaussianAlbedoBuffer[Index];
    float  Roughness = g_GaussianRoughnessBuffer[Index];
    GaussianPBR GaussianData = { Normal, Albedo, Roughness };
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

// Perform frustrum culling (copy pasted from original 3dgs impl)
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

bool IsPointInFrustrum (CameraDescription C, float3 Position, out float3 ViewSpacePosition, bool Ortho = false, float NearClip = 0.f, float Expand = 0.f) {
    ViewSpacePosition = mul(C.View, float4(Position, 1.0f)).xyz;
    // -z axis is aligned with camera direction
    if(ViewSpacePosition.z >= -NearClip) return false;
    float4 Homogeneous = mul(C.Projection, float4(ViewSpacePosition, 1.0f));
    if(Ortho) {
        // FIXME
        return false;
    } else {
        if(Homogeneous.w) {
            float3 Projected = Homogeneous.xyz / Homogeneous.w;
            return all(abs(Projected.xy) < 1.0f + Expand) && Projected.z >= 0.15 && Projected.z <= 1.0f;
        } else {
            return false;
        }
    }
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

// Project the covariance matrix to 2D
// @return The screen space 2D covariance matrix (m00, m01, m11)
float3 ProjectCovarianceMatrixToScreen(float3 mean, float2 focal, float2 tan_fov, SymmetricMatrix Covariance3D, float4x4 View)
{
    float3 t = mul(View, float4(mean, 1.0)).xyz;
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

    float3x3 W = float3x3(
        View[0][0], View[0][1], View[0][2],
        View[1][0], View[1][1], View[1][2],
        View[2][0], View[2][1], View[2][2]);

    float3x3 Mk = mul(J, W);
    
    float3x3 C = ExpandSymmetricMatrix(Covariance3D);
    float3x3 C_2D = mul(mul(Mk, C), transpose(Mk));

    return float3(C_2D[0][0], C_2D[0][1], C_2D[1][1]);
}

// Project the covariance matrix to 2D
// @return The screen space 2D covariance matrix (m00, m01, m11)
float3x3 EWAJacobian (float3 Mean, float2 TanFoV, float4x4 View) {
    float3 P = mul(View, float4(Mean, 1.0)).xyz;
    const float limx = 0.65f * TanFoV.x;
    const float limy = 0.65f * TanFoV.y;
    const float txtz = P.x / P.z;
    const float tytz = P.y / P.z;
    // Clamp to (fov expanded) frustum
    P.x = clamp(txtz, -limx, limx) * P.z;
    P.y = clamp(tytz, -limy, limy) * P.z;

    float3x3 J = float3x3(
        (2 / TanFoV.x) / P.z, 0.0f, - (2 / TanFoV.x) * P.x / (P.z * P.z),
        0.0f, (2 / TanFoV.y) / P.z, - (2 / TanFoV.y) * P.y / (P.z * P.z),
        0.0f, 0.0f, 0.0f);
    return J;
}

float3 ProjectCovarianceMatrixToNDC(float3x3 J, SymmetricMatrix Covariance3D, float4x4 View)
{
    float3x3 W = float3x3(
        View[0][0], View[0][1], View[0][2],
        View[1][0], View[1][1], View[1][2],
        View[2][0], View[2][1], View[2][2]);

    float3x3 Mk = mul(J, W);
    
    float3x3 C = ExpandSymmetricMatrix(Covariance3D);
    float3x3 C_2D = mul(mul(Mk, C), transpose(Mk));

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
    return clamp(Value, 0.0f.xx, 1.f.xx - FLT_EPSILON.xx);
}
float3 saturateDown (float3 Value) {
    return clamp(Value, 0.0f.xxx, 1.f.xxx - FLT_EPSILON.xxx);
}
float4 saturateDown (float4 Value) {
    return clamp(Value, 0.0f.xxxx, 1.f.xxxx - FLT_EPSILON.xxxx);
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

uint2 PackRadianceA16 (float4 Unpacked) {
    uint2 RGBPacked = PackFp16x3Safe(Unpacked.xyz);
    return uint2(
        RGBPacked.x,
        RGBPacked.y | (PackUnorm16(Unpacked.w) << 16)
    );
}

float4 UnpackRadianceA16 (uint2 Packed) {
    float3 RGBUnpacked = UnpackFp16x3(Packed);
    float AUnpacked = UnpackUnorm16(Packed.y >> 16);
    return float4(RGBUnpacked, AUnpacked);
}

uint PackRGBA8 (float4 Unpacked) {
    Unpacked = saturateDown(Unpacked);
    return uint(Unpacked.x * 256.0f) 
        | (uint(Unpacked.y * 256.0f) << 8)
        | (uint(Unpacked.z * 256.0f) << 16)
        | (uint(Unpacked.w * 256.0f) << 24);
}

float4 UnpackRGBA8 (uint Packed) {
    return float4(
        (Packed & 0xFFu) / 256.0f,
        ((Packed >> 8u) & 0xFF) / 256.0f,
        ((Packed >> 16u) & 0xFFu) / 256.0f,
        ((Packed >> 24u) & 0xFFu) / 256.0f
    );
}

struct RayToTrace {
    float3 Direction;
    float3 Origin;
    float  RayTMin;
    float  RayTMax;
    // Specifies if at least 1 hit has been found
    bool   bHit;
    // Ray seed when performing stochastic operations
    float  Seed;
};

// Initialize the ray to trace struct
RayToTrace InitRayToTrace (float RayTMax) {
    RayToTrace Ray = (RayToTrace)0;
    Ray.RayTMin = 0.0f;
    Ray.RayTMax = RayTMax;
    return Ray;
}

// Fetch the ray to trace by its index
RayToTrace FetchRayToTrace (int RayIndex, float RayTMax) {
    RayToTrace Ray = (RayToTrace)0;
    Ray.Direction = Octahedron01ToUnitVector(UnpackUnorm16x2(g_RWRayToTraceDirectionBuffer[RayIndex]));
    Ray.Origin    = g_RWRayToTraceOriginBuffer[RayIndex];
    uint Flags    = g_RWRayToTraceFlagsBuffer[RayIndex];
    Ray.RayTMax   = RayTMax;
    Ray.RayTMin   = asfloat(Flags & RAY_FLAG_TMIN_MASK);
    Ray.bHit      = (Flags & RAY_FLAG_HIT_BIT) != 0;
    Ray.Seed      = g_RWRayToTraceSeedBuffer[RayIndex];
    return Ray;
}

// Write the ray to trace by its index
void WriteRayToTrace (int RayIndex, RayToTrace Ray) {
    g_RWRayToTraceDirectionBuffer[RayIndex] = PackUnorm16x2(UnitVectorToOctahedron01(Ray.Direction));
    g_RWRayToTraceOriginBuffer[RayIndex] = Ray.Origin;
    uint Flags = ((uint)Ray.bHit * RAY_FLAG_HIT_BIT) | asuint(abs(Ray.RayTMin));
    g_RWRayToTraceFlagsBuffer[RayIndex] = Flags;
    g_RWRayToTraceSeedBuffer[RayIndex] = Ray.Seed;
}

void WriteRayTraceFlags (int RayIndex, bool bHit, float TMin) {
    uint Flags = ((uint)bHit * RAY_FLAG_HIT_BIT) | asuint(abs(TMin));
    g_RWRayToTraceFlagsBuffer[RayIndex] = Flags;
}

float4 FetchRayTraceResult (int RayIndex) {
    return UnpackRadianceA16(g_RWRayToTraceResultBuffer[RayIndex]);
}

float FetchRayTraceHitT (int RayIndex) {
    uint Flags = g_RWRayToTraceFlagsBuffer[RayIndex];
    return asfloat(Flags & RAY_FLAG_TMIN_MASK);
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
float EvaluateGaussianResponse (float3 Origin, float3 Direction, Gaussian G, out float RayMaxResponseT) {
    float3x3 InvCov;
    RayMaxResponseT = EvaluateGaussianResponseRayT(Origin, Direction, G, InvCov);
    float3 Position = Origin + RayMaxResponseT * Direction;
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
float3 NDC2ToCameraDirectionUnnormalized (CameraDescription Camera, float2 NDC2) {
    float3 UnnormalizedDirection = Camera.Right * NDC2.x + Camera.Up * NDC2.y + Camera.Direction;
    return UnnormalizedDirection;
}

float3 NDC2ToCameraDirection (CameraDescription Camera, float2 NDC2) {
    return normalize(NDC2ToCameraDirectionUnnormalized(Camera, NDC2));
}

// Spawn a camera ray from the screen position
RayToTrace SpawnCameraRay (CameraDescription Camera, float2 ScreenPosition) {
    RayToTrace Ray = InitRayToTrace(UB.RT_MaxTraceDistance);
    Ray.Origin = Camera.Position;
    float2 NDC2 = ScreenToNDC2(ScreenPosition);
    float3 DirectionUnnormalized = NDC2ToCameraDirectionUnnormalized(Camera, NDC2);
    float  Length = length(DirectionUnnormalized);
    Ray.RayTMin = Length * Camera.NearPlane;
    Ray.RayTMax = Length * Camera.FarPlane;
    Ray.Direction = normalize(DirectionUnnormalized);
    return Ray;
}

uint PackPadScreenPosition (float2 ScreenPosition, float2 InvScreenDimensions) {
    // Scale and pad the borders of the screen space position to make sure it's in the range [0, 1]
    float2 ScaledScreenPosition = saturateDown(.5f * ScreenPosition * InvScreenDimensions + 0.25f);
    return PackUnorm16x2(ScaledScreenPosition);
}

float squared (float Value) {
    return Value * Value;
}

void GetOrthoVectors(in float3 n, out float3 b1, out float3 b2)
{
    bool sel = abs(n.z) > 0;
    float3 p2 = sel ? n : n.zyx;
    float k = 1.0f / sqrt(squared(p2.z) + squared(n.y));
    b1 = float3(0.0f, -p2.z * k, n.y * k);
    b1 = sel ? b1 : b1.zyx;
    b2 = cross(n, b1);
}

bool IsOutOfFilm(CameraDescription C, int2 FilmPosition) {
    return any(FilmPosition < 0) || any(FilmPosition >= C.FilmDimensions);
}
bool IsOutOfFilm(CameraDescription C, uint2 FilmPosition) {
    return any(FilmPosition < 0) || any(FilmPosition >= C.FilmDimensions);
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

// Get the current active camera description
CameraDescription GetCameraDescription () {
    // TODO meshcards
    return UB.MainCamera;
}

Texture2D<float2> GetDepthTexture (CameraDescription C) {
    // TODO meshcards
    return g_GDepthTexture;
}

RWTexture2D<float2> GetRWDepthTexture (CameraDescription C) {
    // TODO meshcards
    return g_RW_GDepthTexture;
}

Texture2D<float4> GetColorTexture (CameraDescription C) {
    // TODO meshcards
    return g_GColorTexture;
}

RWTexture2D<float4> GetRWColorTexture (CameraDescription C) {
    // TODO meshcards
    return g_RW_GColorTexture;
}

Texture2D<float4>  GetNormalTexture (CameraDescription C) {
    // TODO meshcards
    return g_GNormalTexture;
}

float3 GetTexelNormalFromTextureUV (Texture2D<float4> NormalTexture, float2 UV) {
    return normalize(NormalTexture.SampleLevel(g_PointClampSampler, UV, 0.0f).xyz * 2.0f - 1.0f);
}

RWTexture2D<float4> GetRWNormalTexture (CameraDescription C) {
    // TODO meshcards
    return g_RW_GNormalTexture;
}

Texture2D<float> GetFilteredDepthTexture (CameraDescription C) {
    // TODO meshcards
    return g_GFilteredDepthTexture;
}

RWTexture2D<float> GetRWFilteredDepthTexture (CameraDescription C) {
    // TODO meshcards
    return g_RW_GFilteredDepthTexture;
}

Texture2D<float> GetZDepthTexture (CameraDescription C) {
    // TODO meshcards
    return g_GZDepthTexture;
}

RWTexture2D<float> GetRWZDepthTexture (CameraDescription C) {
    // TODO meshcards
    return g_RW_GZDepthTexture;
}

Texture2D<float> GetPreviousZDepthTexture (CameraDescription C) {
    return g_PreviousZDepthTexture;
}

Texture2D<float> GetNearHZBTexture (CameraDescription C) {
    return g_NearHZBTexture;
}

Texture2D<float4> GetRadianceTexture (CameraDescription C) {
    // TODO meshcards
    return g_Radiance;
}

RWTexture2D<float4> GetRWRadianceTexture (CameraDescription C) {
    // TODO meshcards
    return g_RW_Radiance;
}

Texture2D<float4> GetPreviousRadianceTexture (CameraDescription C) {
    return g_PreviousRadiance;
}

float3 RecoverWorldSpacePositionNDC2 (CameraDescription C, float2 NDC2, float LinearDepth) {
    float3 Direction = NDC2ToCameraDirectionUnnormalized(C, NDC2);
    float3 Origin = C.Position;
    float3 WorldSpacePosition = Origin + LinearDepth * Direction;
    return WorldSpacePosition;
}

float3 RecoverWorldSpacePosition (CameraDescription C, float2 FilmPosition, float LinearDepth) {
    float2 NDC2 = FilmToNDC2(C, FilmPosition);
    return RecoverWorldSpacePositionNDC2(C, NDC2, LinearDepth);
}

float  InterpolateBarycentrics (float A, float B, float C, float2 UV) {
    return A * (1 - UV.x - UV.y) + B * UV.x + C * UV.y;
}
float3 InterpolateBarycentrics (float3 A, float3 B, float3 C, float2 UV) {
    return A * (1 - UV.x - UV.y) + B * UV.x + C * UV.y;
}

float LinearToZDepth (CameraDescription C, float LinearDepth) {
    float A = C.Projection[2][2];
    float B = C.Projection[2][3];
    return (-A * LinearDepth + B) / LinearDepth;
}

float ZDepthToLinear (CameraDescription C, float ZDepth) {
    float ZDepthN = 2.0 * ZDepth - 1.0;
    return 2.f * C.NearPlane * C.FarPlane / (C.FarPlane + C.NearPlane - ZDepthN * (C.FarPlane - C.NearPlane));
}

float3 EvaluateSkyRadiance (float3 Direction) {
    return g_EnvironmentMap.SampleLevel(g_LinearWrapSampler, Direction, 0.0f).xyz;
}

// Copy-pasted from UE5. A simple and fast way to get an interleaved gradient noise.

// high frequency dither pattern appearing almost random without banding steps
//note: from "NEXT GENERATION POST PROCESSING IN CALL OF DUTY: ADVANCED WARFARE"
//      http://advances.realtimerendering.com/s2014/index.html
// Epic extended by FrameId
// ~7 ALU operations (2 frac, 3 mad, 2 *)
// @return 0..1
float InterleavedGradientNoise( float2 uv, float FrameId )
{
	// magic values are found by experimentation
	uv += FrameId * (float2(47, 17) * 0.695f);

    const float3 magic = float3( 0.06711056f, 0.00583715f, 52.9829189f );
    return frac(magic.z * frac(dot(uv, magic.xy)));
}

float3 ReprojectToHistoryUVWFromUVW (CameraDescription C, float3 UVW) {
    return TransformPointWithPerspectiveDivide(C.Reprojection, UVW);
}

#endif // INC_3DGS_HLSL
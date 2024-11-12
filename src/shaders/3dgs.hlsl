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

float2 NDC2Screen(float2 NDC) {
    return 0.5f * UB.ScreenDimensions * (NDC + 1.0f);
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

float EvaluateGaussian (Gaussian GaussianData, float3 Position) {
    // x - mu:
    float3 Delta = Position - GaussianData.Position;
    // Transform to gaussian-local space
    // Rotate the position
    float3 RotatedPosition = QuaternionRotate(Delta, GaussianData.Rotation);
    // Scale the position
    float3 ScaledPosition = RotatedPosition / GaussianData.Scale;
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
    return transpose(R);
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

SymmetricMatrix ComputeCovarianceMatrix (float3 Scale, float4 Rotation) {
    float3x3 M = GetScaleRotationTransform(Scale, Rotation);
    // Covariance matrix
    float3x3 Covariance = mul(transpose(M), M);

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

    float3x3 W = float3x3(
        View[0][0], View[0][1], View[0][2],
        View[1][0], View[1][1], View[1][2],
        -View[2][0], -View[2][1], -View[2][2]);

    float3x3 Mk = mul(J, W);

    float3x3 C = ExpandSymmetricMatrix(Covariance3D);
    float3x3 C_2D = mul(Mk, mul(C, transpose(Mk)));

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

float2 UnpackUnorm2x16 (uint Packed) {
    return float2(
        (Packed & 0xFFFF) / 65535.0f,
        (Packed >> 16) / 65535.0f
    );
}

uint PackUnorm2x16 (float2 Unpacked) {
    Unpacked = saturateDown(Unpacked);
    return uint(Unpacked.x * 65536.0f) + (uint(Unpacked.y * 65536.0f) << 16);
}

struct RayToTrace {
    float3 Direction;
    float3 Origin;
    float  RayTMin;
};

RayToTrace FetchRayToTrace (int RayIndex) {
    RayToTrace Ray = (RayToTrace)0;
    float3 Direction = Octahedron01ToUnitVector(UnpackUnorm2x16(g_RWRayToTraceDirectionBuffer[RayIndex]));
    float3 Origin = g_RWRayToTraceOriginBuffer[RayIndex];
    Ray.Direction = Direction;
    Ray.Origin = Origin;
    return Ray;
}

// From the paper.
float EvaluateGaussianResponse (float3 Origin, float3 Direction, Gaussian G) {
    float3x3 MInvC = ExpandSymmetricMatrix(InvC);
    float Numerator = dot(G.Position - Origin, mul(MInvC, Direction));

    float T =  
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

float3 DebugColorHeatMap (float h) {
    float H = saturate(1.0f - h) * 5.0f;
    float R = saturate(min(H - 1.5f, 4.5f - H));
    float G = saturate(min(H - 0.5f, 3.5f - H));
    float B = saturate(min(H + 0.5f, 2.5f - H));
    return float3(R, G, B);
}

#endif // INC_3DGS_HLSL
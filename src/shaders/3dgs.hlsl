#ifndef INC_3DGS_HLSL
#define INC_3DGS_HLSL

#include "../device_shared.hlsl"

#include "3dgs_inc.hlsl"

float2 NDC2Screen(float2 NDC) {
    return 0.5f * UB.ScreenDimensions * (NDC + 1.0f);
}

Gaussian FetchGaussian(uint Index) {
    float3 Position = g_GaussianPositionBuffer[Index];
    float  Alpha = g_GaussianAlphaBuffer[Index];
    float3 Rotation = g_GaussianRotationBuffer[Index];
    asdfsadfdasf// quat??
    float4 Rotation_Quat = float4(Rotation.x, Rotation.y, Rotation.z, 1.0f);
    float3 Scale = g_GaussianScaleBuffer[Index];
    Gaussian GaussianData = { Position, Alpha, Rotation, Scale };
    return GaussianData;
}

SHCoefficents3 FetchGaussianSHCoefficients (int Index, int Degree) {
    SHCoefficents3 SHData = { 0 };
    SHData.Low.Albedo = g_RWGaussianAlbedoBuffer[Index];
    if(Degree >= 1) {
        SHData.Low.SH1[0] = g_RWGaussianSH1Buffer[Index * 3 + 0];
        SHData.Low.SH1[1] = g_RWGaussianSH1Buffer[Index * 3 + 1];
        SHData.Low.SH1[2] = g_RWGaussianSH1Buffer[Index * 3 + 2];
        if(Degree >= 2) {
            SHData.Low.SH2[0] = g_RWGaussianSH2Buffer[Index * 5 + 0];
            SHData.Low.SH2[1] = g_RWGaussianSH2Buffer[Index * 5 + 1];
            SHData.Low.SH2[2] = g_RWGaussianSH2Buffer[Index * 5 + 2];
            SHData.Low.SH2[3] = g_RWGaussianSH2Buffer[Index * 5 + 3];
            SHData.Low.SH2[4] = g_RWGaussianSH2Buffer[Index * 5 + 4];
            if(Degree >= 3) {
                SHData.SH3[0] = g_RWGaussianSH3Buffer[Index * 7 + 0];
                SHData.SH3[1] = g_RWGaussianSH3Buffer[Index * 7 + 1];
                SHData.SH3[2] = g_RWGaussianSH3Buffer[Index * 7 + 2];
                SHData.SH3[3] = g_RWGaussianSH3Buffer[Index * 7 + 3];
                SHData.SH3[4] = g_RWGaussianSH3Buffer[Index * 7 + 4];
                SHData.SH3[5] = g_RWGaussianSH3Buffer[Index * 7 + 5];
                SHData.SH3[6] = g_RWGaussianSH3Buffer[Index * 7 + 6];
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

float3 QuaternionRotate (float3 Vector, float3 Quaternion) {
    float3 q = Quaternion;
    float3 v = Vector;
    return v + 2.0f * cross(q, cross(q, v) + q * dot(q, v));
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

struct CovarianceMatrix {
    float3 Diagonal;
    float3 OffDiagonal;
};

float3x3 EvaluateRotationMatrix (float4 q) {
    float3x3 R = {
        1 - 2 * q.y * q.y - 2 * q.z * q.z, 2 * q.x * q.y - 2 * q.z * q.w, 2 * q.x * q.z + 2 * q.y * q.w,
        2 * q.x * q.y + 2 * q.z * q.w, 1 - 2 * q.x * q.x - 2 * q.z * q.z, 2 * q.y * q.z - 2 * q.x * q.w,
        2 * q.x * q.z - 2 * q.y * q.w, 2 * q.y * q.z + 2 * q.x * q.w, 1 - 2 * q.x * q.x - 2 * q.y * q.y
    };
    return R;
}

// Perform frustrum culling
bool IsInFrustrum (
	float3 Position,
	float3x4 View,
	float3x4 Projection,
	out float3 ViewSpacePosition)
{
	// Bring points to screen space
	float4 Homogeneous = mul(Projection, float4(Position, 1.0f));
	float  InvW = 1.f / (Homogeneous.w + 1e-7f);
	float3 Projected = Homogeneous.xyz * InvW;
    ViewSpacePosition = mul(View, float4(Position, 1.0f)).xyz;

    float Tolerance = 0.2f;
    float ZNear = 0.2f;

    // Check if the point is inside the frustrum
    if(any(Projected.xy < -1.0f - Tolerance) || any(Projected.xy > 1.0f + Tolerance) 
    // Make sure that z is within 0 and 1
    || any(Projected.z < ZNear) || any(Projected.z > 1.0f - Tolerance))
    {
        return false;
    }
	return true;
}

CovarianceMatrix ComputeCovarianceMatrix (float3 Scale, float4 Rotation) {
    // Scaling matrix
    float3x3 S = {
        Scale.x, 0, 0,
        0, Scale.y, 0,
        0, 0, Scale.z
    };
    // Rotation matrix
    float3x3 R = EvaluateRotationMatrix(Rotation);
    float3x3 M = mul(S, R);
    // Covariance matrix
    float3x3 Covariance = mul(transpose(M), M);

    float3 Diagonal = float3(
        Covariance._11,
        Covariance._22,
        Covariance._33
    );
    float3 OffDiagonal = float3(
        Covariance._12,
        Covariance._13,
        Covariance._23
    );
    CovarianceMatrix Ret = (CovarianceMatrix)0;
    Ret.Diagonal = Diagonal;
    Ret.OffDiagonal = OffDiagonal;
    return Ret;
}

// Project the covariance matrix to 2D
// @return The screen space 2D covariance matrix (m00, m01, m11)
float3 ProjectCovarianceMatrixToScreen(float3 mean, float2 focal, float2 tan_fov, CovarianceMatrix Covariance3D, float3x4 View)
{
    float3 t = mul(View, float4(mean, 1.0));

    const float limx = 1.3f * tan_fov.x;
    const float limy = 1.3f * tan_fov.y;
    const float txtz = t.x / t.z;
    const float tytz = t.y / t.z;
    // Clamp to frustum
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

    float3x3 T = mul(W, J);

    float3x3 C = float3x3(
        Covariance3D.Diagonal.x, Covariance3D.OffDiagonal.x, Covariance3D.OffDiagonal.y,
        Covariance3D.OffDiagonal.x, Covariance3D.Diagonal.y, Covariance3D.OffDiagonal.z,
        Covariance3D.OffDiagonal.y, Covariance3D.OffDiagonal.z, Covariance3D.Diagonal.z);

    float3x3 C_2D = mul(transpose(T), mul(transpose(C), T));

    return float3(C_2D[0][0], C_2D[0][1], C_2D[1][1]);
}


// // p(x, mu, sigma) = NormalizationFactor * exp(-0.5 * (x - mu)^T * sigma^-1 * (x - mu))
// // where NormalizationFactor = 1 / sqrt((2 * pi)^3 * det(sigma))
// float EvaluateGaussian (float3 Position, float3 GaussianMu, float3 InvDiagonal, float3 InvCovariance, float3  NormalizationFactor) {
//     // x - mu:
//     float3 Delta = Position - GaussianMu;
//     // sigma^-1:
//     float3x3 InvCovarianceMatrix = {
//         InvDiagonal.x, InvCovariance.x, InvCovariance.y,
//         InvCovariance.x, InvDiagonal.y, InvCovariance.z,
//         InvCovariance.y, InvCovariance.z, InvDiagonal.z
//     };
//     // -0.5 * (x - mu)^T * sigma^-1 * (x - mu):
//     float Exponent = -0.5f * dot(Delta, mul(InvCovarianceMatrix, Delta));
//     return NormalizationFactor * exp(Exponent);
// }

// // Evaluate the Gaussian density at the given position.
// float EvaluateGaussianDensity (GaussianPrecomputed GaussianData, float3 Position) {
//     float Value = EvaluateGaussian(Position, GaussianData.Position, GaussianData.InvDiagonal, GaussianData.InvCovariance, GaussianData.NormalizationFactor);
//     return Value * GaussianData.Alpha;
// }

// // // Evaluate the spherical harmonics with the given direction and SH coefficients.
// // float EvaluateSH (float3 Direction, float SH0, float SH1[3], float SH2[5]) {
// //     float Value = SH0;
    
// // }

// // void SHNewEval3(float fX, float fY, float fZ, out float pSH[9]) {
// //     float fC0, fC1, fS0, fS1, fTmpA, fTmpB, fTmpC;
// //     float fZ2 = fZ * fZ;
// //     pSH[0] = 0.2820947917738781f;
// //     pSH[2] = 0.4886025119029199f * fZ;
// //     pSH[6] = 0.9461746957575601f * fZ2 + -0.3153915652525201f;
// //     fC0 = fX;
// //     fS0 = fY;
// //     fTmpA = -0.48860251190292f;
// //     pSH[3] = fTmpA * fC0;
// //     pSH[1] = fTmpA * fS0;
// //     fTmpB = -1.092548430592079f * fZ;
// //     pSH[7] = fTmpB * fC0;
// //     pSH[5] = fTmpB * fS0;
// //     fC1 = fX * fC0 - fY * fS0;
// //     fS1 = fX * fS0 + fY * fC0;
// //     fTmpC = 0.5462742152960395f;
// //     pSH[8] = fTmpC * fC1;
// //     pSH[4] = fTmpC * fS1;
// // }

// // // Evaluate the Gaussian color with the given view direction.
// // float3 EvaluateGaussianColor (GaussianPrecomputed GaussianData, float3 ViewDirection) {

// // }

#endif // INC_3DGS_HLSL
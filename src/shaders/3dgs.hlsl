#ifndef 3DGS_HLSL
#define 3DGS_HLSL

#include "../device_shared.hlsl"

struct Gaussian {
    // Spatial position
    float3 Position;
    // Alpha
    float Alpha;
    // The upper-right 3 digits of the 3x3 covariance matrix.
    float3 Covariance;
    // Diagonal of the 3x3 covariance matrix. Which specifies its "expansion" in each axis.
    float3 Diagonal;
};

struct GaussianPrecomputed {
    // Spatial position
    float3 Position;
    // Alpha
    float Alpha;
    // The upper-right 3 digits of the 3x3 covariance matrix.
    float3 Covariance;
    // Normalization factor of the gaussian distribution.
    float  NormalizationFactor;
    float3 InvCovariance;
    // Diagonal of the 3x3 covariance matrix. Which specifies its "expansion" in each axis.
    float3 Diagonal;
    float3 InvDiagonal;
};

// SH2
struct SHCoefficients {
    float3 Albedo;
    float3 SH1[3];
    float3 SH2[5];
};

// SH3
struct SHCoefficents3 {
    SHCoefficients SH2Less;
    float3 SH3[7];
};

// Evaluate the normalization factor of the gaussian distribution.
// NormalizationFactor = 1 / sqrt((2 * pi)^3 * det(sigma))
float EvaluateGaussianNormalizationFactor (float3 Covariance, float3 Diagonal) {
    // Calculate the determinant of the covariance matrix.
    // The matrix:
    // Diagonal.x, Covariance.x, Covariance.y
    // Covariance.x, Diagonal.y, Covariance.z
    // Covariance.y, Covariance.z, Diagonal.z

    float3x3 CovarianceMatrix = {
        Diagonal.x, Covariance.x, Covariance.y,
        Covariance.x, Diagonal.y, Covariance.z,
        Covariance.y, Covariance.z, Diagonal.z
    };
    const float A = float(1.0 / sqrt(8.0 * M_PI * M_PI * M_PI));
    return A * (1.0f / sqrt(determinant(CovarianceMatrix)));
}

// p(x, mu, sigma) = NormalizationFactor * exp(-0.5 * (x - mu)^T * sigma^-1 * (x - mu))
// where NormalizationFactor = 1 / sqrt((2 * pi)^3 * det(sigma))
float EvaluateGaussian (float3 Position, float3 GaussianMu, float3 InvDiagonal, float3 InvCovariance, float3  NormalizationFactor) {
    // x - mu:
    float3 Delta = Position - GaussianMu;
    // sigma^-1:
    float3x3 InvCovarianceMatrix = {
        InvDiagonal.x, InvCovariance.x, InvCovariance.y,
        InvCovariance.x, InvDiagonal.y, InvCovariance.z,
        InvCovariance.y, InvCovariance.z, InvDiagonal.z
    };
    // -0.5 * (x - mu)^T * sigma^-1 * (x - mu):
    float Exponent = -0.5f * dot(Delta, mul(InvCovarianceMatrix, Delta));
    return NormalizationFactor * exp(Exponent);
}

// Evaluate the Gaussian density at the given position.
float EvaluateGaussianDensity (GaussianPrecomputed GaussianData, float3 Position) {
    float Value = EvaluateGaussian(Position, GaussianData.Position, GaussianData.InvDiagonal, GaussianData.InvCovariance, GaussianData.NormalizationFactor);
    return Value * GaussianData.Alpha;
}

// // Evaluate the spherical harmonics with the given direction and SH coefficients.
// float EvaluateSH (float3 Direction, float SH0, float SH1[3], float SH2[5]) {
//     float Value = SH0;
    
// }

// void SHNewEval3(float fX, float fY, float fZ, out float pSH[9]) {
//     float fC0, fC1, fS0, fS1, fTmpA, fTmpB, fTmpC;
//     float fZ2 = fZ * fZ;
//     pSH[0] = 0.2820947917738781f;
//     pSH[2] = 0.4886025119029199f * fZ;
//     pSH[6] = 0.9461746957575601f * fZ2 + -0.3153915652525201f;
//     fC0 = fX;
//     fS0 = fY;
//     fTmpA = -0.48860251190292f;
//     pSH[3] = fTmpA * fC0;
//     pSH[1] = fTmpA * fS0;
//     fTmpB = -1.092548430592079f * fZ;
//     pSH[7] = fTmpB * fC0;
//     pSH[5] = fTmpB * fS0;
//     fC1 = fX * fC0 - fY * fS0;
//     fS1 = fX * fS0 + fY * fC0;
//     fTmpC = 0.5462742152960395f;
//     pSH[8] = fTmpC * fC1;
//     pSH[4] = fTmpC * fS1;
// }

// // Evaluate the Gaussian color with the given view direction.
// float3 EvaluateGaussianColor (GaussianPrecomputed GaussianData, float3 ViewDirection) {

// }

#endif
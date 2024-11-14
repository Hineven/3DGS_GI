#ifndef INC_3DGS_SHARED_HLSL
#define INC_3DGS_SHARED_HLSL

#include "../device_shared.hlsl"

#define TILE_SIZE 16

#define SMALL_TILE_SIZE 8

// Keep aligned with C++ side
// Each empty line indicates the end of evey 16 bytes packed in the struct.
struct UniformBlock {
    float4x4 View;

    float4x4 Projection;

    float4x4 ViewProjection;

    float3 CameraPosition;
    int   NumGaussians;

    // Number of pixels per film unit
    //     ScreenDimension / (2 * tan(FOV / 2))
    float2 CameraFocal;
    // 2 * Tan(FOV / 2)
    float2 CameraFieldOfView;

    // Camera ray: normalize(CameraRight * NDC.x + CameraUp * NDC.y + CameraDirection)
    float3 CameraRight; // Not normalized. Affected by FOV
    float  CameraNearPlane;

    float3 CameraUp; // Not normalized. Affected by FOV
    float  CameraFarPlane;

    float3 CameraDirection; // Normalized
    // Paper says 0.3 will be good.
    // Smaller values will make the proxy geometry for gaussians smaller.
    float GaussianRTProxyGeometrySigma;

    // Dimension of the screen (in pixels)
    int2  ScreenDimensions;
    // Dimension of the screen (in 16x16 tiles)
    int2  TileDimensions;

    // (Default) Thread group size for indirect dispatched shader
    int IndirectThreadGroupSize;
    // Min alpha we want our gaussians to have to be evaluated
    // NOTE: Only used in the ray tracing shader.
    float MinAlphaForGaussianEvaluation;
    // Dimension of the screen (in small 8x8 tiles)
    int2 SmallTileDimensions;
};

struct Gaussian {
    // Spatial position
    float3 Position;
    // Alpha
    float Alpha;
    // Rotation
    float4 Rotation;
    // Scale
    float3 Scale;
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
    float3 Color;
    float3 SH1[3];
    float3 SH2[5];
};

// SH3
struct SHCoefficents3 {
    SHCoefficients Low;
    float3 SH3[7];
};

#endif // INC_3DGS_SHARED_HLSL
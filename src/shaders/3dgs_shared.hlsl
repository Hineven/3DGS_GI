#ifndef INC_3DGS_SHARED_HLSL
#define INC_3DGS_SHARED_HLSL

#include "../device_shared.hlsl"

#define TILE_SIZE 16

struct UniformBlock {
    float4x4 View;
    float4x4 Projection;
    float4x4 ViewProjection;

    float3 CameraPosition;
    uint   NumGaussians;

    // Number of pixels per film unit
    // def fov2focal(fov, pixels):
    //     return pixels / (2 * math.tan(fov / 2))
    float2 Focal;
    // 2 * Tan(fov / 2)
    float2 FieldOfView;

    float  NearPlane;
    float  FarPlane;

    // Dimension of the screen (in pixels)
    uint2  ScreenDimensions;

    // Dimension of the screen (in tiles)
    uint2  TileDimensions;
    // (Default) Thread group size for indirect dispatched shader
    uint IndirectThreadGroupSize;
    uint Padding;
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
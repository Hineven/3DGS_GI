#ifndef INC_3DGS_SHARED_HLSL
#define INC_3DGS_SHARED_HLSL

#include "device_shared.hlsl"

#define TILE_SIZE 16

#define SMALL_TILE_SIZE 8

struct CameraDescription {
    // View space: rhs, camera direction aligned to -z
    float4x4 View;
    // NDC: lhs, camera direction aligned to z, [-1, 1]*[-1, 1]*[0, 1]
    float4x4 Projection;
    float4x4 ProjectionView;

    float3 Position;
    float  NearPlane;

    float3 Direction;
    float  FarPlane;

    // FilmDimension / (2 * tan(FOV / 2))
    // Meaningless if the camera is orthographic.
    float2 Focal;
    // 2 * Tan(FOV / 2)
    // Meaningless if the camera is orthographic.
    float2 FieldOfView;

    int2   FilmDimensions;
    float2 InvFilmDimensions;

    float3 Right;
    uint   Flags;

    float3 Up;
    uint Padding0;
};


// Keep aligned with C++ side
// Each empty line indicates the end of evey 16 bytes packed in the struct.
struct UniformBlock {
    CameraDescription MainCamera;

    int   NumGaussians;
    // Paper says 0.3 will be good.
    // Smaller values will make the proxy geometry for gaussians smaller.
    float GaussianRTProxyGeometrySigma;
    // (Default) Thread group size for indirect dispatched shader
    int IndirectThreadGroupSize;
    // Min alpha we want our gaussians to have to be evaluated
    // NOTE: Only used in the ray tracing shader.
    float MinAlphaForGaussianEvaluation;

    // Dimension of the screen (in pixels), the same as MainCamera.FilmDimensions
    int2  ScreenDimensions;
    // Dimension of the screen (in 16x16 tiles)
    int2  TileDimensions;

    // Dimension of the screen (in small 8x8 tiles)
    int2 SmallTileDimensions;
    float RT_AlphaMultiplier;
    uint  Padding0;
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
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
    // Current UVW(UVZ) -> Previous UVW (UVZ)
    float4x4 Reprojection;

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

    float2 FilmTexelSize;
    // Size of each pixel on HZB buffer at mip 0
    float2 HZBBaseTexelSize;
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
    float HWRT_MinAlphaForGaussianEvaluation;

    // Dimension of the screen (in pixels), the same as MainCamera.FilmDimensions
    int2  ScreenDimensions;
    // Dimension of the screen (in 16x16 tiles)
    int2  TileDimensions;

    // Dimension of the screen (in small 8x8 tiles)
    int2 SmallTileDimensions;
    float HWRT_AlphaMultiplier;
    int  FrameIndex;

    uint DebugMode;
    // whether we're tracing and visualizing shading rays (started from the camera) 
    // Otherwise only shadow ray depths are (possibly) visualized.
    uint VisualizeShadingRays;
    // Pixels with alpha values larger or equal to this threshold will be considered opaque.
    float OpaqueThreshold;
    // Alphas for blending depth values are reduced by a factor of this value.
    // Thus there can be smoother depth transitions between different gaussians.
    float DepthAlphaClipValue;

    float HWRT_StochasticRayTracingQuality;
    float RT_MaxTraceDistance;
    float SSRT_MaxTraceDistance;
    float SSRT_RelativeTexelThickness;
    
    float3 Debug_LightPosition;
    uint   SSRT_MaxNumIterations;

    float4 LightGrid_GridCascadeMin[4];
    
    float4 LightGrid_GridCascadeMax[4];

    float3 LightGrid_GridSize;
    uint   LightGrid_GridResolution;

    uint   LightGrid_GridResolution2;
    uint   LightGrid_GridResolution3;
    uint   LightGrid_NumGridCascades;
    float  LightGrid_MinLightContributionForInjection;

    float  LightGrid_MinResampleWeightForDirectIllumiation;
    uint   LightGrid_MaxNumEntries; // Max number of entries in the light grids
    uint   DI_OcclusionThresholdMinFactor;
    uint   DI_OcclusionThresholdMaxFactor;

    int2   Debug_CursorPixelCoords;
    int2   Padding0_2;

    DeviceVirtualAddressRange RT_RayGenerationShaderRecord;
    
    DeviceVirtualAddressRangeAndStride RT_MissShaderTable;
    uint2 Padding3;

    DeviceVirtualAddressRangeAndStride RT_HitGroupTable;
    uint2 Padding4;
    
    DeviceVirtualAddressRangeAndStride RT_CallableShaderTable;
    uint2 Padding5;
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

struct GaussianPBR {
    // Normal
    float3 Normal;
    // PBR related stuff
    float3 Albedo;
    float  Roughness;
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

#define LIGHT_GRID_MAX_NUM_CASCADES 4

#define LIGHT_GRID_MAX_NUM_GRID_LIGHTS 8

#define LIGHT_GRID_MAX_GRID_SIZE 32

#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_SKY  1
#define LIGHT_TYPE_AREA 2

#endif // INC_3DGS_SHARED_HLSL
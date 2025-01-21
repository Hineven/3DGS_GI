#ifndef INC_3DGS_SHARED_HLSL
#define INC_3DGS_SHARED_HLSL

#include "device_shared.hlsl"

#define INVALID_U32 (0xffffffffu)

#define TILE_SIZE 16

#define SMALL_TILE_SIZE 8

#define CAMERA_TYPE_PERSPECTIVE 0
#define CAMERA_TYPE_ORTHOGRAPHIC 1

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
    uint   Padding;

    float2 FilmTexelSize;
    // Size of each pixel on HZB buffer at mip 0
    float2 HZBBaseTexelSize;

    int2   HZBDimensions;
    float2 InvHZBDimensions;

    // Converting from screen uv to hzb uv
    float4 UVToHZB_ScaleOffset;
    // Converting from hzb uv to screen uv
    float4 HZBToUV_ScaleOffset;
};

INLINE int GetCameraType (CameraDescription C) {
    // Lowest 4 bits specifies the camera type
    return C.Flags & 0xf;
}

// Keep aligned with C++ side
// Each empty line indicates the end of evey 16 bytes packed in the struct.
struct UniformBlock {
    CameraDescription MainCamera;
    CameraDescription PreviousMainCamera;

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

    float2 InvScreenDimensions;
    float2 InvTileDimensions;

    // Dimension of the screen (in small 8x8 tiles)
    int2 SmallTileDimensions;
    float HWRT_AlphaMultiplier;
    int  FrameIndex;

    uint DebugMode;
    // whether we're tracing and visualizing shading rays (started from the camera) 
    // Otherwise only shadow ray depths are (possibly) visualized.
    uint HWRT_VisualizeShadingRays;
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

    float  LightGrid_GridSize;
    uint   LightGrid_GridResolution;
    uint   LightGrid_GridResolution2;
    uint   LightGrid_GridResolution3;

    uint   LightGrid_NumGridCascades;
    float  LightGrid_MinLightContributionForInjection;
    float  LightGrid_MinResampleWeightForDirectIllumiation;
    uint   LightGrid_MaxNumEntries; // Max number of entries in the light grids
    
    float  DI_OcclusionThresholdMinFactor;
    float  DI_OcclusionThresholdMaxFactor;
    float  DI_FilterGaussianRadius;
    float  DI_InvFilterGaussianRadius2;

    float  DI_Denoiser_DepthThreshold;
    uint   DI_NoTemporalDenoising;
    uint   DI_NoSpatialDenoising;
    float  DI_Denoiser_TargetNumSamples;

    uint   II_NoTemporalDenoising;
    float  II_Denoiser_TargetNumSamples;
    float  II_SecondaryVertexNormalOffset;
    float  II_SecondaryVertexRadianceClamping;

    float  FallbackReflection_Denoiser_TargetNumSamples;
    float  Reflection_InvFilterRadius;
    float  DI_InvFilterGaussianRadius;
    uint   SSRC_ProbeFiltering;

    uint   SSRC_NoImportanceSampling;
    uint   SSRC_NumUniformScreenProbes;
    uint   SSRC_BaseUpdateRayWaves;
    uint   SSRC_ResetCache;

    uint   SSRC_MaxNumAdaptiveProbes;
    uint   SSRC_NoAdaptiveProbes;
    uint   SSRC_TileJitterFrameSeed;
    uint   SSRC_PreviousTileJitterFrameSeed;

    float2 TAAJitterUV;
    uint   HashGrids_MaxNumSamples;
    uint   HashGrids_MaxNumTiles;

    float3 HashGrids_Center;
    float  HashGrids_InvCascadeRadius;

    float  HashGrids_CellSize;
    uint   HashGrids_NumBuckets;
    uint   HashGrids_NumInterleavedEntriesPerBucket;
    uint   HashGrids_TargetSampleCount;

    uint   HashGrids_TileLifespan;
    uint   HashGrids_MaxNumEntriesSearchedPerBucket;
    float2 PreviousTAAJitterUV;

    float  Reflection_MaxRoughness;
    float  Reflection_FilterRadius;
    float  Reflection_InvFilterRadius2;
    uint   FallbackReflection_NoTemporalDenoising;

    int    DepthFilterRadius;
    float  GaussianClampingScale;
    // Mesh cards will be allocated according to this value (world space size). 
    // (no reallocation if the instance transform changes)
    float  Card_PreferredTexelWorldSize;
    uint   Debug_CardSetToVisualize;

    float  Card_SampleZDepthVisibilityBias;
    float  Card_MinCardViewDirectionWeightToSample;
    float  Card_GaussianClampingScale;
    float  TonemapExposure;

    float  Denoiser_Reflection_MaxSampleCount;
    float  Reflection_MaxSampleRoughness;
    uint   UseReconstructedNormals;
    float  OriginalNormalWeight;

    uint   NoDirectDiffuseIllumination;
    uint   NoIndirectDiffuseIllumination;
    uint   NoReflection;
    float  SSRT_RayContinuationBackwardBiasFactor;
    
    float ProbeNormalWeightFactor;
    float GaussianExpandFactor;
    float SSRC_ProbeTemporalBlendFactor;
    uint  II_EnvironmentOnly;

    uint  HWRT_VisualizeStochasticRays;
    uint  HWRT_ShadeWithSphericalHarmonics;
    uint  Paddingx01;
    uint  Paddingx02;
    
    float  LightingSkyRadianceLOD;
    uint   Debug_VisualizeLightGridCascade;
    int2   Debug_CursorPixelCoords;

    uint   Debug_VisualizeMeshCardScene;
    uint   Debug_VisualizeMeshCardAtlas;
    uint   Debug_VisualizeMeshCardAtlasChannel;
    uint   Debug_VisualizeMeshCardAtlasLayer;

    float2 Debug_VisualizeMeshCardAtlasOffset;
    float  Debug_VisualizeMeshCardAtlasScale;
    uint   Debug_SSRC_VisualizeProbeUpdateRays;
    
    uint   Debug_SSRC_VisualizeProbes;
    uint   SSRC_ProbeUpdateRaySampleSeed;
    uint   SSRC_FixedProbeUpdateRaySampleSeed;
    uint   TonemapMode;

    uint   EnableAA;
    float  SkyLightMultiplier;
    uint   Padding1;
    uint   Padding2;

    DeviceVirtualAddressRange RT_RayGenerationShaderRecord;
    
    DeviceVirtualAddressRangeAndStride RT_MissShaderTable;
    uint2 Padding3;

    DeviceVirtualAddressRangeAndStride RT_HitGroupTable;
    uint2 Padding4;
    
    DeviceVirtualAddressRangeAndStride RT_CallableShaderTable;
    uint2 Padding5;
};

struct DrawMeshCardUniformBlock {
    CameraDescription Camera;

    int3 CardAtlasBaseCoords;
    int InstanceIndex;

    int CardIndex;
    int CardSetIndex;
    uint Padding0;
    uint Padding1;
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

struct Light {
    // 3 types, directional, sky, area
    uint Type;
    // Infomation used to approximate light contribution to grids
    float  Intensity;
    // Grid index
    int4   GridIndex;
    // Grid local position, [0, 1)^3 (low precision)
    float3 LocalPosition;
    // Light normal / direction (low precision)
    float3 Normal;
    bool bInvalid;
};

struct LightData {
	float3 V1; // Light direction if this is a directional light. Meaningless if this is a sky light.
	float3 V2; // Only used for area lights.
	float3 V3; // Only used for area lights.
	float3 Radiance;
};

#ifndef __cplusplus
#define SEMANTIC(x) : x
#else
#define SEMANTIC(x)
#endif

struct Vertex {
    float3 Position SEMANTIC(POSITION);
    float3 Normal SEMANTIC(NORMAL);
    float2 TexCoord SEMANTIC(TEXCOORD1);
};

// Instance space card set
struct CardSet {
    float3 MinBounds;
    float3 MaxBounds;
    int  CardIndexBase;
    int3 NumCards; // x, y, z, a multiple of 2
    int3 CardResolutions; // yz, xz, xy for 3 axises
};

// Very-very simple material for regular meshes
struct SimpleMaterial {
    float3 Albedo;
    float  Roughness;
    float3 Emissive;
    float  Padding;
};

#define MIN_CARD_RESOLUTION_L2 4
#define MIN_CARD_RESOLUTION 16
#define MAX_CARD_RESOLUTION 128
#define CARD_ATLAS_RESOLUTION 4096
#define NUM_CARD_ATLAS 8
// 4 bits for each axis
#define MAX_NUM_CARDS_PER_AXIS 16


struct Card {
    // z = 0xff means invalid card
    int3 AtlasBaseCoords;
};

#define LIGHT_GRID_MAX_NUM_CASCADES 4

#define LIGHT_GRID_MAX_NUM_GRID_LIGHTS 8

#define LIGHT_GRID_MAX_GRID_SIZE 32

#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_SKY  1
#define LIGHT_TYPE_AREA 2

#define SSRC_PROBE_TEXTURE_SIZE 8
#define SSRC_PROBE_TEXTURE_TEXEL_COUNT (SSRC_PROBE_TEXTURE_SIZE * SSRC_PROBE_TEXTURE_SIZE)
#define SSRC_PROBE_TEXTURE_TEXEL_COUNT_L2 6
#if SSRC_PROBE_TEXTURE_TEXEL_COUNT != (1 << SSRC_PROBE_TEXTURE_TEXEL_COUNT_L2)
#error "inconsistent SSRC_PROBE_TEXTURE_TEXEL_COUNT and SSRC_PROBE_TEXTURE_TEXEL_COUNT_L2"
#endif
#define SSRC_PROBE_NORMAL_OFFSET (1e-4f)

#define SSRC_MAX_NUM_UPDATE_RAY_PER_PROBE 128

#define SSRC_MAX_ADAPTIVE_PROBE_LAYERS 2

#define HASHGRIDS_TILE_CELL_WIDTH 8
#define HASHGRIDS_MAX_NUM_ENTRIES_SEARCHED_PER_BUCKET 8
#define HASHGRIDS_TILE_CELL_MIP_OFFSET_0 0
#define HASHGRIDS_TILE_CELL_MIP_OFFSET_1 (HASHGRIDS_TILE_CELL_WIDTH * HASHGRIDS_TILE_CELL_WIDTH)
#define HASHGRIDS_TILE_CELL_MIP_OFFSET_2 (HASHGRIDS_TILE_CELL_MIP_OFFSET_1 + (HASHGRIDS_TILE_CELL_MIP_OFFSET_1 / 4))
#define HASHGRIDS_TILE_CELL_MIP_OFFSET_3 (HASHGRIDS_TILE_CELL_MIP_OFFSET_2 + (HASHGRIDS_TILE_CELL_MIP_OFFSET_1 / 16))
#define HASHGRIDS_NUM_CELLS_PER_TILE (HASHGRIDS_TILE_CELL_MIP_OFFSET_3 + 1)

// Can not be greater than a device limit.
#define RT_INSTANCE_REGULAR_MESH_BIT 0x8000u

#endif // INC_3DGS_SHARED_HLSL
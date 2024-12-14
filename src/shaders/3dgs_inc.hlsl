#ifndef INC_3DGS_INC_HLSL
#define INC_3DGS_INC_HLSL

#include "../3dgs_shared.hlsl"

#define INVALID_U32 (0xffffffffu)

#define ZDEPTH_INF (1.f + FLT_EPSILON)

#ifndef WAVE_SIZE
// Make code linting work
#define WAVE_SIZE 32
#error "WAVE_SIZE must be defined"
#endif

// Maximum number of closest hits to cache per ray trace.
// Consistent with the 3DGS ray tracing paper.
#define HIT_BUFFER_SIZE 16

// Output the colored gaussians in the output channel.
// For visualization purposes only.
#define OUTPUT_COLORED_GAUSSIANS

// Use better normal reconstruction algorithm (5 samples required vs 4 samples)
// #define HIGH_QUALITY_NORMAL_RECONSTRUCTION

// Bitpack the vertex attributes when transfering them to the fragment shader
// #define BITPACK_VERTEX_ATTRIBUTES

// Output full G-Buffers for PBR rendering
// Controlled by the renderer.
// #define OUTPUT_PBR_G_BUFFER

// Use depth to reconstruct the normals, instead of rendering them directly
// Controlled by the renderer.
// #define RECONSTRUCT_NORMALS_FROM_DEPTH

// Acceleration structure for hardware ray tracing
//                      TLAS
// BLAS0(GS Group), BLAS1(GS Group), BLAS2(Mesh), ... 
RaytracingAccelerationStructure g_HWRT_AccelerationStructure;

// ----------------- Scene -----------------
// Skybox
TextureCube<float4> g_EnvironmentMap;
// Index the offset of gaussians of each instance in the acceleration structure
// BLAS0 Gaussian indices: Offset[0], Offset[0]+1, Offset[0]+2, ..., Offset[0] + Count[0] - 1
// BLAS1 Gaussian indices: Offset[1], Offset[1]+1, Offset[1]+2, ..., Offset[1] + Count[1] - 1
StructuredBuffer<uint> g_InstanceGaussianIndexOffsetBuffer;
// Number of gaussians in each instance
StructuredBuffer<uint> g_InstanceGaussianCountBuffer;
// Transforms of the gaussian instances (GS groups) (to world space)
StructuredBuffer<float3x4> g_InstanceTransformBuffer;
// Inverse transforms of the gaussian instances (GS groups) (to instance/local space)
StructuredBuffer<float3x4> g_InstanceInvTransformBuffer;
// Normal transforms of the gaussian instances (GS groups) (to world space)
StructuredBuffer<float3x3> g_InstanceNormalTransformBuffer;
// AABBs of the gaussian instances (GS groups)
// StructuredBuffer<float3>   g_InstanceAABBMinBuffer;
// StructuredBuffer<float3>   g_InstanceAABBMaxBuffer;

// The buffer holding all the gaussian positions (instance local coordinates)
StructuredBuffer<float3> g_GaussianPositionBuffer;
// The buffer holding all the gaussian alphas
StructuredBuffer<float>  g_GaussianAlphaBuffer;
// The buffer holding all the gaussian rotations (quaternions)
StructuredBuffer<float4> g_GaussianRotationBuffer;
// The buffer holding all the gaussian scales (xyz)
StructuredBuffer<float3> g_GaussianScaleBuffer;

// SH Coefficients
StructuredBuffer<float3> g_GaussianColorBuffer;
// Stride 3
StructuredBuffer<float3> g_GaussianSH1Buffer;
// Stride 5
StructuredBuffer<float3> g_GaussianSH2Buffer;
// Stride 7
StructuredBuffer<float3> g_GaussianSH3Buffer;

// PBR related stuff
StructuredBuffer<float3> g_GaussianAlbedoBuffer;
StructuredBuffer<float>  g_GaussianRoughnessBuffer;

StructuredBuffer<float3> g_GaussianNormalBuffer;
// End of scene related stuff

// Store indirect dispatch commands
RWStructuredBuffer<DispatchRaysIndirectCommand> g_RWDispatchRaysIndirectCommandBuffer;
StructuredBuffer<uint> g_RaysToDispatchCountBuffer;

RWStructuredBuffer<DispatchIndirectCommand> g_RWDispatchIndirectCommandBuffer;
StructuredBuffer<uint> g_ThreadsToDispatchCountBuffer;

RWStructuredBuffer<DrawIndirectCommand> g_RWDrawIndirectCommandBuffer;
StructuredBuffer<uint> g_VertexToDrawCountBuffer;

// Number of active gaussians. Updated every frame.
RWStructuredBuffer<uint>  g_RWActiveGaussianCountBuffer;
// List of active gaussian indices (unsorted)
RWStructuredBuffer<uint>  g_RWActiveGaussianListSrcBuffer;
// Depths of the active gaussians (used for sorting keys)
RWStructuredBuffer<float> g_RWActiveGaussianLinearDepthSrcBuffer;
// RWStructuredBuffer<float> g_RWActiveGaussianLinearDepthSortedBuffer;
// Sorted active gaussian list (sorted by depth)
RWStructuredBuffer<uint>  g_RWActiveGaussianListBuffer;
RWStructuredBuffer<float> g_RWActiveGaussianLinearDepthBuffer;

// Screen positions of the active gaussians, 1/2 packed (sorted)
RWStructuredBuffer<uint>   g_RWActiveGaussianNDCPositionBuffer;
RWStructuredBuffer<uint>   g_RWActiveGaussianQuadNDCVector0Buffer;
RWStructuredBuffer<uint>   g_RWActiveGaussianQuadNDCVector1Buffer;

// Precomputed color values for active gaussians
// Only used when OUTPUT_COLORED_GAUSSIANS is defined
RWStructuredBuffer<float3>  g_RWActiveGaussianColorBuffer;


// Raytracing related stuff

// States of rays to be traced (or during tracing)
// Number of rays in total
RWStructuredBuffer<uint>   g_RWRayCountBuffer;
// Number of rays to be traced.
RWStructuredBuffer<uint>   g_RWRayToTraceCountBuffer;
RWStructuredBuffer<uint>   g_RWCompactedRayToTraceCountBuffer;
// Active ray indices to be traced
RWStructuredBuffer<uint>   g_RWRayToTraceListBuffer;
RWStructuredBuffer<uint>   g_RWCompactedRayToTraceListBuffer;
// Octahedron encoded ray direction
RWStructuredBuffer<uint>   g_RWRayToTraceDirectionBuffer;
// Ray origins
RWStructuredBuffer<float3> g_RWRayToTraceOriginBuffer;
// Sometimes we use the film positions of ray origins. unorm16x2 packed
RWStructuredBuffer<uint> g_RWRayToTraceUVPositionBuffer; 
// Random number seed of the ray
RWStructuredBuffer<float>  g_RWRayToTraceSeedBuffer;
// Ray flags, including the hit flag and TMin
RWStructuredBuffer<uint>   g_RWRayToTraceFlagsBuffer;

// We found a closest hit and the ray is completed tracing.
#define RAY_FLAG_HIT_BIT 0x80000000u
// The lower 31 bits packs the seed of the ray (unsigned float)
#define RAY_FLAG_TMIN_MASK (0x7fffffffu)

// Keep the result (rgba) of the traced ray, RGBA16 Packed
// Only written to in shading ray tracing, used to visualize the ray tracing scene.
RWStructuredBuffer<uint2> g_RWRayToTraceResultBuffer;

// Direct illumination required buffers
// Threshould for shadow ray occlusion tests.
RWStructuredBuffer<float> g_RWDirectIlluminationRayOcclusionThresholdBuffer;
// Contribution of the direct illumination for each ray (fp16 packed)
RWStructuredBuffer<uint2>  g_RWDirectIlluminationRayContributionBuffer;

// G-Buffers
RWTexture2D<float4>      g_RW_GColorTexture;
Texture2D<float4>        g_GColorTexture;
RWTexture2D<float2>       g_RW_GDepthTexture;
// Linear depth, not Z buffer depth
Texture2D<float2>         g_GDepthTexture;

// Roughness, nx, ny, unused
RWTexture2D<float4>      g_RW_GMaterialTexture;
Texture2D<float4>        g_GMaterialTexture;
RWTexture2D<float4>      g_RW_GNormalTexture;
Texture2D<float4>        g_GNormalTexture;
RWTexture2D<float>       g_RW_GFilteredDepthTexture;
Texture2D<float>         g_GFilteredDepthTexture;

RWTexture2D<float4>      g_RW_DebugTexture;
Texture2D<float4>        g_DebugTexture;

// ZDepth is reconstructed from the filtered depth texture.
RWTexture2D<float>        g_RW_GZDepthTexture;
Texture2D<float>          g_GZDepthTexture;
Texture2D<float>          g_HistoryZDepthTexture;
// HiZ buffer
Texture2D<float>          g_NearHZBTexture;

// Output buffers
RWTexture2D<float4>      g_RW_DirectIllumination;
Texture2D<float4>        g_DirectIllumination;
Texture2D<float4>        g_HistoryDirectIllumination;
RWTexture2D<float4>      g_RW_FilteredDirectIllumination;
Texture2D<float4>        g_FilteredDirectIllumination;
RWTexture2D<float4>      g_RW_Radiance;
Texture2D<float4>        g_Radiance;
Texture2D<float4>        g_HistoryRadiance;


// All non-resource uniforms
ConstantBuffer<UniformBlock> UB;

SamplerState g_LinearClampSampler;
SamplerState g_LinearWrapSampler;
SamplerState g_PointClampSampler;
SamplerState g_PointWrapSampler;

// Output buffers for ray tracing proxy mesh building
RWStructuredBuffer<float3> g_RW_RTVertexBuffer;
RWStructuredBuffer<uint>   g_RW_RTIndexBuffer;

#ifndef NDEBUG
// Buffers for debugging purposes
// Mapping from pixel index to ray index for direct illumination occlusion tests
RWStructuredBuffer<uint> g_Debug_DirectIlluminationPixelRayIndexBuffer;
// Ray visualization
RWStructuredBuffer<uint>   g_Debug_VisualizeRayCountBuffer;
RWStructuredBuffer<float3> g_Debug_VisualizeRayVertexBuffer;
RWStructuredBuffer<float3> g_Debug_VisualizeRayColorBuffer;
RWStructuredBuffer<uint>   g_Debug_VisualizeRayRayIndexBuffer; // This buffer keeps the indices for the visualized rays

#endif

#endif // INC_3DGS_INC_HLSL

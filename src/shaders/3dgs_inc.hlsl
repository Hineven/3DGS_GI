#ifndef INC_3DGS_INC_HLSL
#define INC_3DGS_INC_HLSL

#include "3dgs_shared.hlsl"

#define INVALID_U32 (0xffffffffu)

#ifndef WAVE_SIZE
// Make code linting work
#define WAVE_SIZE 32
#error "WAVE_SIZE must be defined"
#endif

// Maximum number of closest hits to cache per ray trace.
// Consistent with the paper.
#define HIT_BUFFER_SIZE 16

// Acceleration structure for hardware ray tracing
//                      TLAS
// BLAS0(GS Group), BLAS1(GS Group), BLAS2(Mesh), ... 
RaytracingAccelerationStructure g_HWRT_AccelerationStructure;
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

// Store indirect dispatch commands
RWStructuredBuffer<DispatchIndirectCommand> g_RWDispatchIndirectCommandBuffer;
StructuredBuffer<uint> g_ThreadsToDispatchCountBuffer;

// Number of active gaussians. Updated every frame.
RWStructuredBuffer<uint>  g_RWGaussianActiveCountBuffer;
// List of active gaussian indices
RWStructuredBuffer<uint>  g_RWActiveGaussianListBuffer;
// Their corresponding depths
RWStructuredBuffer<float> g_RWActiveGaussianDepthBuffer;
// Screen positions of the active gaussians
RWStructuredBuffer<float2> g_RWActiveGaussianScreenPositionBuffer;
// Screen radius
RWStructuredBuffer<float>  g_RWActiveGaussianScreenRadiusBuffer;
// Conic screen space filter kernel, and extra opacity W
RWStructuredBuffer<float4> g_RWActiveGaussianConicWBuffer;
// Number of tiles overlapped by each active gaussian
RWStructuredBuffer<uint>   g_RWActiveGaussianTileCountBuffer;
// Scan sum of g_RWActiveGaussianTileCountBuffer
RWStructuredBuffer<uint>   g_RWActiveGaussianInstanceBaseBuffer;
// Number of (active) gaussian instances
RWStructuredBuffer<uint>   g_RWActiveGaussianInstanceCountBuffer;
// Evaluated color for active gaussians
RWStructuredBuffer<float3> g_RWActiveGaussianColorBuffer;

// Sort key (uint32) of each gaussian instance 
RWStructuredBuffer<uint>   g_RWActiveGaussianInstanceKeyBuffer;
RWStructuredBuffer<uint>   g_RWActiveGaussianInstanceKeySortedBuffer;
// // Indirect sorting index
// RWStructuredBuffer<uint>   g_RWActiveGaussianInstanceSortIndexBuffer;
// Refers to the original gaussian inside active gaussian list from gaussian instance
RWStructuredBuffer<uint>   g_RWActiveGaussianInstanceGaussianIndexBuffer;
// Sorted.
RWStructuredBuffer<uint>   g_RWActiveGaussianInstanceGaussianIndexSortedBuffer;

// The index of the starting gaussian in the sorted list for each tile [l, r)
RWStructuredBuffer<uint>   g_RWTileGaussianInstanceStartBuffer; 
RWStructuredBuffer<uint>   g_RWTileGaussianInstanceEndBuffer;

// States of rays to be traced (or during tracing)
// Number of rays to be traced.
RWStructuredBuffer<uint>   g_RWRayToTraceCountBuffer;
// Octahedron encoded ray direction
RWStructuredBuffer<uint>   g_RWRayToTraceDirectionBuffer;
// Ray origins
RWStructuredBuffer<float3> g_RWRayToTraceOriginBuffer;
// The tmax of the ray to trace
RWStructuredBuffer<float>  g_RWRayToTraceTMaxBuffer;
// Ray flags
RWStructuredBuffer<uint>   g_RWRayFlagsBuffer;

// The ray is completed. A closest hit is found or nothing is found in the range.
#define RAY_FLAG_COMPLETED_BIT 0x1u
// At least 1 hit is found along the ray
#define RAY_FLAG_HIT_FOUND_BIT 0x2u
// The lower 8 bits are the current accumulated opacity of geometries along the ray
#define RAY_FLAG_OPACITY_MASK (0xffu)

// Keep the result (rgba) of the traced ray, RGBA16 Packed
RWStructuredBuffer<uint2> g_RWRayToTraceResultBuffer;


// G-Buffers
RWTexture2D<float4>      g_RW_GColorTexture;
// Linear depth texture, which is dot(HitPosition - CameraPosition, CameraDirection)
RWTexture2D<float>       g_RW_GLinearDepthTexture;
RWTexture2D<float4>      g_RW_GAlbedoTexture;

Texture2D<float4>        g_GColorTexture;

// All non-resource uniforms
ConstantBuffer<UniformBlock> UB;

SamplerState g_LinearClampSampler;
SamplerState g_LinearWrapSampler;

// Output buffers for ray tracing proxy mesh building
RWStructuredBuffer<float3> g_RW_RTVertexBuffer;
RWStructuredBuffer<uint>   g_RW_RTIndexBuffer;

#endif // INC_3DGS_INC_HLSL

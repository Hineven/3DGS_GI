#ifndef INC_3DGS_INC_HLSL
#define INC_3DGS_INC_HLSL

#include "3dgs_shared.hlsl"

#ifndef WAVE_SIZE
// Make code linting work
#define WAVE_SIZE 32
#error "WAVE_SIZE must be defined"
#endif

// The buffer holding all the gaussian positions
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

// The index of the starting gaussian in the sorted list for each tile
RWStructuredBuffer<uint>   g_RWTileGaussianInstanceStartBuffer; 

RWTexture2D<float4>      g_RW_GColorTexture;
Texture2D<float4>        g_GColorTexture;

// All non-resource uniforms
ConstantBuffer<UniformBlock> UB;

SamplerState g_LinearClampSampler;
SamplerState g_LinearWrapSampler;

#endif // INC_3DGS_INC_HLSL

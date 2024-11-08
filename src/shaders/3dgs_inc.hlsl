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
StructuredBuffer<float3> g_GaussianRotationBuffer;
// The buffer holding all the gaussian scales (xyz)
StructuredBuffer<float3> g_GaussianScaleBuffer;

// Coarse radius of projected gaussians in screen space
StructuredBuffer<float> g_RWGaussianCoarseRadiusBuffer;
// Number of screen tiles overlapped by each gaussian
StructuredBuffer<uint>  g_RWGaussianNumTilesOverlappedBuffer;

// SH Coefficients
StructuredBuffer<float3> g_RWGaussianAlbedoBuffer;
// Stride 3
StructuredBuffer<float3> g_RWGaussianSH1Buffer;
// Stride 5
StructuredBuffer<float3> g_RWGaussianSH2Buffer;
// Stride 7
StructuredBuffer<float3> g_RWGaussianSH3Buffer;

// Number of active gaussians. Updated every frame.
StructuredBuffer<uint>  g_RWGaussianActiveCountBuffer;
// List of active gaussian indices
StructuredBuffer<uint>  g_RWActiveGaussianListBuffer;
// Their corresponding depths
StructuredBuffer<float> g_RWActiveGaussianDepthBuffer;
// Screen positions of the active gaussians
StructuredBuffer<float2> g_RWActiveGaussianScreenPositionBuffer;
// Screen radius
StructuredBuffer<float>  g_RWActiveGaussianScreenRadiusBuffer;
// Conic screen space filter kernel, and extra opacity W
StructuredBuffer<float>  g_RWActiveGaussianConicWBuffer;
// Number of tiles overlapped by each active gaussian
StructuredBuffer<uint>   g_RWActiveGaussianTileCountBuffer;
// Scan sum of g_RWActiveGaussianTileCountBuffer
StructuredBuffer<uint>   g_RWActiveGaussianInstanceBaseBuffer;
// Evaluated color for active gaussians
StructuredBuffer<float3> g_RWActiveGaussianColorBuffer;

// Sort key (uint32) of each gaussian instance 
StructuredBuffer<uint>   g_RWActiveGaussianInstanceKeyBuffer;
// Indirect sorting index
StructuredBuffer<uint>   g_RWActiveGaussianInstanceSortIndexBuffer;
// Refers to the original gaussian inside active gaussian list from gaussian instance
StructuredBuffer<uint>   g_RWActiveGaussianInstanceGaussianIndexBuffer;

// The index of the starting gaussian in the sorted list for each tile
StructuredBuffer<uint>   g_RWTileGaussianInstanceStartBuffer; 

RWTexture2D<float4>      g_RW_GColorBuffer;

// All non-resource uniforms
ConstantBuffer<UniformBlock> UB;

#endif // INC_3DGS_INC_HLSL

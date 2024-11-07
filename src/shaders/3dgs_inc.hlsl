#ifndef INC_3DGS_INC_HLSL
#define INC_3DGS_INC_HLSL

#include "3dgs_shared.hlsl"

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

// All non-resource uniforms
ConstantBuffer<UniformBlock> UB;

#endif // INC_3DGS_INC_HLSL

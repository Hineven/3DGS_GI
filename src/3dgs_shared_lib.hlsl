#ifndef INC_3DGS_SHARED_LIB_HLSL
#define INC_3DGS_SHARED_LIB_HLSL

#include "3dgs_shared.hlsl"

float Luminance_Shared (float3 Radiance) {
    return dot(Radiance, float3(0.33333333f, 0.33333333f, 0.33333333f));
}

#endif
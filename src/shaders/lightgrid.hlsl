#ifndef LIGHT_GRID_HLSL
#define LIGHT_GRID_HLSL
#include "../3dgs_shared.hlsl"
#include "3dgs_inc.hlsl"
#include "transforms.hlsl"
#include "sampling.hlsl"
#include "conventions.hlsl"

#include "material.hlsl"

// Scene-related lighting info (these buffers are set in device_scene.cpp)
// Number of lights
RWStructuredBuffer<uint>  g_LightCountBuffer;
RWStructuredBuffer<uint2> g_LightBuffer;
// 12 x 4 bytes per light
RWStructuredBuffer<float3> g_LightDataBuffer;
// Bloom filter alike technique to reuse occlusion infomation
// Stores the set of the grids that the light is successfully sampled from.
// RWStructuredBuffer<uint>  g_LightGridHashBuffer;
// Grid hash values (binary bitmask for bloom filter)
// RWStructuredBuffer<uint>  g_LightGrid_GridHashBuffer;

int GetNumLights () {
    return g_LightCountBuffer[0];
}

struct Light {
    // 3 types, directional, sky, area
    uint Type;
    // Infomation used to approximate light contribution to grids
    float  Intensity;
    // Grid index
    int4   GridIndex;
    // Grid local position (low precision)
    float3 LocalPosition;
    // Light normal / direction (low precision)
    float3 Normal;
};

Light UnpackLightHeader (uint2 Packed) {
    Light L = (Light)0;
    // Grid index: 6 x 3 + 2 = 20 bits 
    L.GridIndex = int4(
        Packed.x & 0x3f,
        (Packed.x >> 6) & 0x3f,
        (Packed.x >> 12) & 0x3f,
        (Packed.x >> 18) & 0x3
    );
    // Grid local position: 3 x 4 bits
    L.LocalPosition = float3(
        (Packed.x >> 20) & 0xf,
        (Packed.x >> 24) & 0xf,
        (Packed.x >> 28) & 0xf
    );
    // Light type: 2 bits
    L.Type = Packed.y & 0x3;
    // Light normal: 2 x 7 bits
    float2 Oct01 = float2(
        (Packed.y >> 2) & 0x7f,
        (Packed.y >> 9) & 0x7f
    );
    L.Normal = Octahedron01ToUnitVector((Oct01 + 0.5f) / 128.f);
    // Intensity: fp16
    L.Intensity = asfloat(Packed.y >> 16);
    return L;
}

Light FetchLightHeader (int LightIndex) {
    return UnpackLightHeader(g_LightBuffer[LightIndex]);
}
// Allocator for light grid list
RWStructuredBuffer<uint> g_LightGrid_GridLightListAllocator;
// Number of lights in each grid
RWStructuredBuffer<uint> g_LightGrid_GridLightCountBuffer;
// Offset of light list rank in each grid
RWStructuredBuffer<uint> g_LightGrid_GridLightListOffsetBuffer;
// Sum of the estimated contribution of lights that overlaped with the grid
// Seems unnecessary. RIS does not require the source distribution to be normalized
// RWStructuredBuffer<float> g_LightGrid_GridLightSumWeightBuffer;
// Light indices in each grid
RWStructuredBuffer<uint> g_LightGrid_GridLightListBuffer;


uint LightGrid_GetGridIndex1 (uint4 GridIndex) {
    return (GridIndex.x + GridIndex.y * UB.LightGrid_GridResolution
         + GridIndex.z * UB.LightGrid_GridResolution2) + GridIndex.w * UB.LightGrid_GridResolution3;
}

uint4 LightGrid_GetGridIndex (uint GridIndex1) {
    uint4 GridIndex;
    GridIndex.w = GridIndex1 / UB.LightGrid_GridResolution3;
    GridIndex1 -= GridIndex.w * UB.LightGrid_GridResolution3;
    GridIndex.z = GridIndex1 / UB.LightGrid_GridResolution2;
    GridIndex1 -= GridIndex.z * UB.LightGrid_GridResolution2;
    GridIndex.y = GridIndex1 / UB.LightGrid_GridResolution;
    GridIndex.x = GridIndex1 - GridIndex.y * UB.LightGrid_GridResolution;
    return GridIndex;
}

uint2 PackLightHeader (Light L) {
    uint2 Packed = 0;
    // Type: 2 bits
    Packed.x = L.Type << 30;
    // Grid index: 6 x 4 = 24 bits
    Packed.x |= (L.GridIndex.x & 0x3f) << 6;
    Packed.x |= (L.GridIndex.y & 0x3f) << 12;
    Packed.x |= (L.GridIndex.z & 0x3f) << 18;
    Packed.x |= (L.GridIndex.w & 0x3f) << 24;
    // Grid local position: 3 x 6 bits
    Packed.x |= uint(L.LocalPosition.x * 64.f) & 0x3f;
    Packed.y |= uint(L.LocalPosition.y * 64.f) & 0x3f;
    Packed.y |= uint(L.LocalPosition.z * 64.f) << 6;
    // Intensity: fp16
    Packed.y |= asuint(L.Intensity) << 16;
    return Packed;
}

// returns the wold grid min
float3 LightGrid_GetGridBounds (int4 GridIndex, out float GridSize) {
    GridSize = UB.LightGrid_GridSize * pow(2, GridIndex.w);
    return UB.LightGrid_GridCascadeMin[GridIndex.w].xyz 
         + GridSize * GridIndex.xyz;
}

bool LightGrid_IsInsideGrid (int4 GridIndex, float3 Position) {
    float3 GridSize;
    float3 GridMin = LightGrid_GetGridBounds(GridIndex, GridSize);
    return all(Position >= GridMin) && all(Position < GridMin + GridSize);
}

bool LightGrid_IsInsideAnyCascade (float3 Position) {
    // Check if the position is inside the largest cascade of grids
    return all(Position >= UB.LightGrid_GridCascadeMin[UB.LightGrid_NumGridCascades - 1].xyz) 
        && all(Position < UB.LightGrid_GridCascadeMax[UB.LightGrid_NumGridCascades - 1].xyz);
}

int4 LightGrid_GetGridIndex (float3 Position) {
    float GridSize = UB.LightGrid_GridSize;

    [unroll(LIGHT_GRID_MAX_NUM_CASCADES)]
    for(int i = 0; i < UB.LightGrid_NumGridCascades; i ++) {
        if(all(Position >= UB.LightGrid_GridCascadeMin[i].xyz) 
         && all(Position < UB.LightGrid_GridCascadeMax[i].xyz)) {
            return int4(
                int3((Position - UB.LightGrid_GridCascadeMin[i].xyz) / GridSize),
                i
            );
        }
        GridSize = GridSize * 2;
    }
    return int4(-1, -1, -1, -1);
}

float3 SampleSkyLight (float3 Normal, float2 u, out float Pdf) {
    float3 LocalDirection = CosineWeightedSampleHemisphere(u);
    float3 Tangent, Bitangent;
    GetOrthoVectors(Normal, Tangent, Bitangent);
    Pdf = CosineWeightedSampleHemispherePDF(LocalDirection.z);
    return Tangent * LocalDirection.x + Bitangent * LocalDirection.y + Normal * LocalDirection.z; 
}

float3 SampleAreaLightArea (float3 V0, float3 V1, float3 V2, float2 u, out float AreaPdf) {
    if(u.x + u.y > 1.f) {
        u = 1.f - u;
    }
    float3 Position = InterpolateBarycentrics(V0, V1, V2, u);
    AreaPdf = 1.f / length(cross(V1 - V0, V2 - V0));
    return Position;
}

float3 GetLightWorldPosition (Light L) {
    float3 GridSize;
    float3 GridMin = LightGrid_GetGridBounds(L.GridIndex, GridSize);
    return GridMin + L.LocalPosition * GridSize;
}

// A coarse estimtion used for light grid injection
float EstimateLightGridContribution (Light L, float3 GridMin, float GridSize) {
    if(L.Type == LIGHT_TYPE_DIRECTIONAL || L.Type == LIGHT_TYPE_SKY) {
        return L.Intensity;
    } else if (L.Type == LIGHT_TYPE_AREA) {
        // Estimate the contribution from the area light using appriximated solid angle

        // Calculate the distance from the light to the grid
        float3 LightPosition = GetLightWorldPosition(L);
        float3 GridCenter    = GridMin + GridSize * 0.5f;
        // Offset the light position according to the light normal for conservative estimation
        LightPosition -= L.Normal * GridSize * sqrt(0.75f);
        float3 Direction     = normalize(GridCenter - LightPosition);
        float  Distance      = length(GridCenter - LightPosition);

        // Assume that the light is small enough compared to the grid, estimate the solid angle.
        float CosineFactor   = saturate(dot(L.Normal, Direction));
        float SolidAngle     = CosineFactor / (Distance * Distance);
        return L.Intensity * SolidAngle;
    }
}

bool IsInfinitelyFarLightType (uint Type) {
    if(Type == LIGHT_TYPE_DIRECTIONAL || Type == LIGHT_TYPE_SKY) {
        return true;
    }
    return false;
}

#endif
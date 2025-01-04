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
// This buffer is updated from LightDataBuffer via a kernel that generates the light list every frame
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
    ) * (1 / 16.f) + (1 / 32.f);
    // Light type: 2 bits
    L.Type = Packed.y & 0x3;
    // Light normal: 2 x 7 bits
    float2 Oct0128 = float2(
        (Packed.y >> 2) & 0x7f,
        (Packed.y >> 9) & 0x7f
    );
    L.Normal = Octahedron01ToUnitVector((Oct0128 + 0.5f) / 128.f);
    // Intensity: fp16
    L.Intensity = f16tof32(Packed.y >> 16);
    L.bInvalid  = L.Intensity < 0;
    return L;
}

Light FetchLightHeader (int LightIndex) {
    return UnpackLightHeader(g_LightBuffer[LightIndex]);
}

LightData FetchLightDetails (int LightIndex) {
	LightData Details = (LightData) 0;
	int Offset = LightIndex * 4;
	Details.V1 = g_LightDataBuffer[Offset + 0];
	Details.V2 = g_LightDataBuffer[Offset + 1];
	Details.V3 = g_LightDataBuffer[Offset + 2];
	Details.Radiance = g_LightDataBuffer[Offset + 3];
	return Details;
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
// Sum of weights for all sampled lights with each light grid.
RWStructuredBuffer<float> g_LightGrid_GridReservoirWeightBuffer;


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
    // Grid index: 6 x 3 + 2 = 20 bits
    Packed.x |= (L.GridIndex.x & 0x3f);
    Packed.x |= (L.GridIndex.y & 0x3f) << 6;
    Packed.x |= (L.GridIndex.z & 0x3f) << 12;
    Packed.x |= (L.GridIndex.w & 0x3)  << 18;
    // Grid local position: 3 x 4 bits
    Packed.x |= uint(saturateDown(L.LocalPosition.x) * 16.f) << 20;
    Packed.x |= uint(saturateDown(L.LocalPosition.y) * 16.f) << 24;
    Packed.x |= uint(saturateDown(L.LocalPosition.z) * 16.f) << 28;
    // Light type: 2 bits
    Packed.y |= L.Type & 0x3;
    // Light normal: 2 x 7 bits
    uint2 Oct01 = uint2(saturateDown(UnitVectorToOctahedron01(L.Normal)) * 128);
    Packed.y |= (Oct01.x << 2) | (Oct01.y << 9);
    // Intensity: fp16
    Packed.y |= f32tof16(L.bInvalid ? -1 : L.Intensity) << 16;
    return Packed;
}

void WriteLightHeader (uint LightIndex, Light L) {
    g_LightBuffer[LightIndex] = PackLightHeader(L);
}

// returns the wold grid min
float3 LightGrid_GetGridBounds (int4 GridIndex, out float GridSize) {
    GridSize = UB.LightGrid_GridSize * pow(2, GridIndex.w);
    return UB.LightGrid_GridCascadeMin[GridIndex.w].xyz 
         + GridSize * GridIndex.xyz;
}

bool LightGrid_IsInsideGrid (int4 GridIndex, float3 Position) {
    float  GridSize;
    float3 GridMin = LightGrid_GetGridBounds(GridIndex, GridSize);
    return all(Position >= GridMin) && all(Position < GridMin + GridSize);
}

bool LightGrid_IsInsideAnyCascade (float3 Position) {
    // Check if the position is inside the largest cascade of grids
    return all(Position >= UB.LightGrid_GridCascadeMin[UB.LightGrid_NumGridCascades - 1].xyz) 
        && all(Position < UB.LightGrid_GridCascadeMax[UB.LightGrid_NumGridCascades - 1].xyz);
}

uint4 LightGrid_GetGridIndex (float3 Position) {
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
    return INVALID_U32.xxxx;
}

float3 SampleSkyLight (float3 Normal, float2 u, out float Pdf) {
    float3 LocalDirection = CosineWeightedSampleHemisphere(u);
    float3 Tangent, Bitangent;
    GetOrthoVectors(Normal, Tangent, Bitangent);
    Pdf = CosineWeightedSampleHemispherePdf(LocalDirection.z);
    return Tangent * LocalDirection.x + Bitangent * LocalDirection.y + Normal * LocalDirection.z; 
}

float3 SampleAreaLightArea (float3 V0, float3 V1, float3 V2, float2 u, out float AreaPdf) {
    if(u.x + u.y > 1.f) {
        u = 1.f - u;
    }
    float3 Position = InterpolateBarycentrics(V0, V1, V2, u);
    AreaPdf = 2.f / length(cross(V1 - V0, V2 - V0));
    return Position;
}

float3 GetLightWorldPosition (Light L) {
    float  GridSize;
    float3 GridMin = LightGrid_GetGridBounds(L.GridIndex, GridSize);
    return GridMin + L.LocalPosition * GridSize;
}

// A coarse estimtion used for light grid injection
float EstimateLightGridContribution (Light L, float3 GridMin, float GridSize) {
    if(L.Type == LIGHT_TYPE_DIRECTIONAL || L.Type == LIGHT_TYPE_SKY) {
        // The estimated irradiance is constant accross the scene
        // for sky and directional lights
        return L.Intensity; // Irradiance
    } else if (L.Type == LIGHT_TYPE_AREA) {
        // Estimate the contribution from the area light using appriximated solid angle
        // Here, L.Intensity is the luminance of the light x the area of the light

        // Calculate the distance from the light to the grid
        float3 LightPosition = GetLightWorldPosition(L);
        float3 GridCenter    = GridMin + GridSize * 0.5f;
        // Offset the light position according to the light normal for conservative estimation
        float3 OffsetedLightPosition = LightPosition;// - L.Normal * GridSize * sqrt(3.f);
        float  VolumeFactor = 1.f;//saturate((dot(GridCenter - LightPosition, L.Normal) + sqrt(0.75f)) / sqrt(3.f));
        float3 Direction     = normalize(GridCenter - OffsetedLightPosition);
        float  Distance      = length(GridCenter - LightPosition);

        // Assume that the light is small enough compared to the grid, estimate the solid angle.
        float CosineFactor   = saturate(dot(L.Normal, Direction));
        float SolidAngle     = CosineFactor / max(Distance * Distance, 1e-6f);

        // TODO there're seemingly artifacts rendering small lights far away from camera 
        return L.Intensity * SolidAngle * VolumeFactor;
    }
    // This should never happen
    return 0.f;
}

bool IsInfinitelyFarLightType (uint Type) {
    if(Type == LIGHT_TYPE_DIRECTIONAL || Type == LIGHT_TYPE_SKY) {
        return true;
    }
    return false;
}

#endif
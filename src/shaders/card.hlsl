#ifndef CARD_HLSL
#define CARD_HLSL
#include "../3dgs_shared.hlsl"
#include "3dgs_inc.hlsl"
#include "transforms.hlsl"
#include "math.hlsl"

// Uploaded from host
StructuredBuffer<float4> g_CardSets;

CardSet FetchCardSet (int CardSetIndex) {
    float4 V1 = g_CardSets[CardSetIndex * 2];
    float4 V2 = g_CardSets[CardSetIndex * 2 + 1];
    CardSet Result = (CardSet)0;
    Result.MinBounds = V1.xyz;
    Result.MaxBounds = float3(V1.w, V2.xy);
    uint V2z = asuint(V2.z);
    Result.CardIndexBase = V2z & 0xffffu;
    Result.NumCards = int3(
        (V2z >> 16) & 0xfu,
        (V2z >> 20) & 0xfu,
        (V2z >> 24) & 0xfu
    );
    // 4 bit unused.
    uint V2w = asuint(V2.w);
    Result.CardResolutions = MIN_CARD_RESOLUTION * pow(2.xxx, int3(
        (V2w) & 0xfu,
        (V2w >> 4) & 0xfu, 
        (V2w >> 8) & 0xfu
    ));
    // many bits unused
    return Result;
}

// Uploade from host
StructuredBuffer<uint> g_Cards;

Card FetchCard (int CardIndex) {
    uint V = g_Cards[CardIndex];
    Card Result = (Card)0;
    Result.AtlasBaseCoords = int3(
        V & 0xfffu,
        (V >> 12) & 0xfffu,
        V >> 24
    );
    return Result;
}

RWTexture2DArray<float3> g_RWCardAtlas_ColorTexture;
Texture2DArray<float3> g_CardAtlas_ColorTexture;
RWTexture2DArray<float> g_RWCardAtlas_AlphaTexture;
Texture2DArray<float>  g_CardAtlas_AlphaTexture;
RWTexture2DArray<float3> g_RWCardAtlas_NormalTexture;
Texture2DArray<float3> g_CardAtlas_NormalTexture;
// All cards are shaded as lamberitan surfaces
// Texture2D<float>  g_CardAtlas_MaterialTexture;
// Card space linear depth.
RWTexture2DArray<float> g_RWCardAtlas_LinearDepthTexture;
Texture2DArray<float>  g_CardAtlas_LinearDepthTexture;
// Lighting
RWTexture2DArray<float3> g_RWCardAtlas_DirectIlluminationTexture;
Texture2DArray<float3> g_CardAtlas_DirectIlluminationTexture;
RWTexture2DArray<float3> g_RWCardAtlas_IndirectIlluminationTexture;
Texture2DArray<float3> g_CardAtlas_IndirectIlluminationTexture;
// Sum of direct and indirect illumination
RWTexture2DArray<float3> g_RWCardAtlas_LightingTexture;
Texture2DArray<float3> g_CardAtlas_LightingTexture;


RWTexture2D<float4> g_RWCardWorkspace_ColorAlbedoTexture;
RWTexture2D<float4> g_RWCardWorkspace_NormalTexture;
RWTexture2D<float2> g_RWCardWorkspace_LinearDepthTexture;

struct CardSample {
    uint CardIndex;
    Card SampledCard;
    int    AtlasIndex;
    float2 AtlasGatherUV;
    float4 BillinearWeights; // 00, 01, 10, 11, gather order: wzxy
    bool   bValid;
};

CardSample SampleCard (
    uint CardIndex, Card CardHeader, int2 CardDimensions, float2 BoundsMin, float2 BoundsMax, float2 CardSpaceProjection) {
    float2 LocalUV = saturateDown((CardSpaceProjection - BoundsMin) / (BoundsMax - BoundsMin));
    float2 LocalFilmPosition = clamp(LocalUV * CardDimensions, 0.5f, CardDimensions - 0.5f);
    float2 AtlasPosition = LocalFilmPosition + CardHeader.AtlasBaseCoords.xy;
    float2 AtlasBillinearPosition = floor(AtlasPosition - 0.5f);
    float2 BillinearWeights = frac(AtlasPosition - 0.5f);
    float2 AtlasGatherUV = (AtlasBillinearPosition + 1.f) * UB.InvCardAtlasDimensions;
    float4 GatherWeights = float4(
        (1 - BillinearWeights.x) * (1 - BillinearWeights.y),
        BillinearWeights.x * (1 - BillinearWeights.y),
        (1 - BillinearWeights.x) * BillinearWeights.y,
        BillinearWeights.x * BillinearWeights.y
    );


    CardSample Result = (CardSample)0;
    Result.CardIndex = CardIndex;
    Result.SampledCard = CardHeader;
    Result.AtlasIndex = CardHeader.AtlasBaseCoords.z;
    Result.AtlasGatherUV = AtlasGatherUV;
    Result.BillinearWeights = GatherWeights;
    Result.bValid = CardHeader.AtlasBaseCoords.z != 0xffu;
    return Result;
}

float3 SampleCardAtlas (Texture2D Atlas, float2 AtlasGatherUV, float4 Weights) {
    float4 Red = Atlas.GatherRed(g_PointClampSampler, AtlasGatherUV, AtlasGatherUV).wzxy;
    float4 Green = Atlas.GatherGreen(g_PointClampSampler, AtlasGatherUV, AtlasGatherUV).wzxy;
    float4 Blue = Atlas.GatherBlue(g_PointClampSampler, AtlasGatherUV, AtlasGatherUV).wzxy;
    return float3(
        dot(Red, Weights),
        dot(Green, Weights),
        dot(Blue, Weights)
    );
}

int GetCardRank (float3 AxisPositions, int3 CardSetNumCards, float3 ViewDirectionSign, float3 BoundsMin, float3 BoundsMax, int Axis) {
    int NumCards = CardSetNumCards[Axis] / 2;
    bool bFlipOrders = ViewDirectionSign[Axis] < 0;
    int CardRankR = bFlipOrders ? NumCards : 0;
    float L = bFlipOrders ? BoundsMax[Axis] : BoundsMin[Axis];
    float R = bFlipOrders ? BoundsMin[Axis] : BoundsMax[Axis];
    float AxisPosition = bFlipOrders ? -AxisPositions[Axis] : AxisPositions[Axis];
    for(int i = 0; i < NumCards; i ++) {
        float T = (float)i / NumCards;
        float CardAxisPosition = lerp(L, R, T);
        if(CardAxisPosition > AxisPosition) {
            break;
        }
        CardRankR ++;
    }
    CardRankR = clamp(CardRankR, 1, NumCards);
    return CardRankR - 1;
}

struct CardSampleAccumulator {
    float Opacity;
    float3 DirectIllumination;
    float3 IndirectIllumination;
    float3 Lighting;
    float  WeightSum;
};

void AccumulateCardSample (CardSample Sample, float HitDepth, float WorldToInstanceScale, inout CardSampleAccumulator Accuumulator) {
    if(!Sample.bValid) {
        return;
    }
    float4 Depths = g_CardAtlas_LinearDepthTexture.GatherRed(g_PointClampSampler, Sample.AtlasGatherUV).wzxy * WorldToInstanceScale;
    // UE5 method of biasing
    float BiasThreshold = CacheBias / ;
    float4 VisibilityWeights = 1.0f - saturate((abs(HitDepth - Depths) - BiasThreshold) / BiasFalloff);
    float4 Weights = Sample.BillinearWeights * VisibilityWeights;
    float3 Direct = SampleCardAtlas(g_CardAtlas_DirectIlluminationTexture, Sample.AtlasGatherUV, Weights);
    float3 Indirect = SampleCardAtlas(g_CardAtlas_IndirectIlluminationTexture, Sample.AtlasGatherUV, Weights);
    float3 Lighting = SampleCardAtlas(g_CardAtlas_LightingTexture, Sample.AtlasGatherUV, Weights);
    float  Opacity  = SampleCardAtlas(g_CardAtlas_AlphaTexture, Sample.AtlasGatherUV, Weights).a;

    Accuumulator.Opacity += Opacity * ;

}

CardSetSample SampleCardSet (
    CardSet Cards,
    float3x4 WorldToInstanceTransform,
    float3x3 WorldToInstanceNormalTransform,
    float3 WorldPosition,
    float3 WorldViewDirection
) {
    float3 LocalPosition      = TransformPoint(WorldToInstanceTransform, WorldPosition);
    LocalPosition = clamp(LocalPosition, Cards.MinBounds, Cards.MaxBounds);
    float3 LocalViewDirection = normalize(TransformVector(WorldToInstanceNormalTransform, WorldViewDirection));
    float3 ViewDirectionWeights = abs(LocalViewDirection);
    ViewDirectionWeights = 1.f / dot(1.f.xxx, ViewDirectionWeights);
    // yz plane
    if(ViewDirectionWeights.x > UB.Cards_MinCardViewDirectionWeightToSample) {
        int CardRank = GetCardRank(
            LocalPosition, Cards.NumCards, LocalViewDirection, Cards.MinBounds, Cards.MaxBounds, 0
        );
        int CardIndex = CardRank + Cards.CardIndexBase;
        Card CardHeader = FetchCard(CardIndex);
        CardSample Sample = SampleCard(
            CardIndex, CardHeader, Cards.CardResolutions.yz, Cards.MinBounds.yz, Cards.MaxBounds.yz, LocalPosition.yz
        );
        AccumulateSample(Sample, Accumulator);
    }
}


#endif
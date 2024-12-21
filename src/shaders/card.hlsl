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

Texture2D<float4> g_CardWorkspace_ColorAlphaTexture;
RWTexture2D<float4> g_RWCardWorkspace_ColorAlphaTexture;
Texture2D<float4> g_CardWorkspace_NormalTexture;
RWTexture2D<float4> g_RWCardWorkspace_NormalTexture;
Texture2D<float2> g_CardWorkspace_LinearDepthTexture;
RWTexture2D<float2> g_RWCardWorkspace_LinearDepthTexture;

Texture2D<float4> g_CardWorkspace_DirectIlluminationTexture;
RWTexture2D<float4> g_RWCardWorkspace_DirectIlluminationTexture;
Texture2D<float4> g_CardWorkspace_HistoryDirectIlluminationTexture;
Texture2D<float4> g_CardWorkspace_IndirectIlluminationTexture;
RWTexture2D<float4> g_RWCardWorkspace_IndirectIlluminationTexture;
Texture2D<float4> g_CardWorkspace_HistoryIndirectIlluminationTexture;
Texture2D<float4> g_CardWorkspace_LightingTexture;
RWTexture2D<float4> g_RWCardWorkspace_LightingTexture;
Texture2D<float4> g_CardWorkspace_HistoryLightingTexture;

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
    float2 AtlasGatherUV = (AtlasBillinearPosition + 1.f) * (1.f / CARD_ATLAS_RESOLUTION);
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

float3 SampleCardAtlas3 (Texture2DArray<float3> Atlas, float3 GatherPosition, float4 Weights) {
    float4 Red = Atlas.GatherRed(g_PointClampSampler, GatherPosition).wzxy;
    float4 Green = Atlas.GatherGreen(g_PointClampSampler, GatherPosition).wzxy;
    float4 Blue = Atlas.GatherBlue(g_PointClampSampler, GatherPosition).wzxy;
    return float3(
        dot(Red, Weights),
        dot(Green, Weights),
        dot(Blue, Weights)
    );
}
float SampleCardAtlas (Texture2DArray<float> Atlas, float3 GatherPosition, float4 Weights) {
    float4 Red = Atlas.GatherRed(g_PointClampSampler, GatherPosition).wzxy;
    return dot(Red, Weights);
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

    float3 SolveRadiance () {
        return Lighting / max(WeightSum, 1e-6f);
    }
};

void AccumulateCardSample (CardSample Sample, 
    float HitDepth, float InstanceZScale, int NumZAxisCards, float ViewDirectionWeight,
    inout CardSampleAccumulator Accumulator) {
    if(!Sample.bValid) {
        return;
    }
    float3 GatherPosition = float3(Sample.AtlasGatherUV, Sample.AtlasIndex);
    float4 Depths = g_CardAtlas_LinearDepthTexture.GatherRed(
        g_PointClampSampler, GatherPosition
    ).wzxy;
    float BiasThreshold = InstanceZScale / NumZAxisCards * UB.Card_SampleZDepthVisibilityBias;
    float BiasFalloff = BiasThreshold * 0.25f;
    // UE5 method of biasing
    float4 VisibilityWeights = 1.0f - saturate((abs(HitDepth - Depths) - BiasThreshold) / BiasFalloff);
    VisibilityWeights *= select(Depths > 0, 1, 0); // Filter out invalid depths
    float4 Weights = Sample.BillinearWeights * VisibilityWeights;
    float  SampleWeight = dot(Weights, 1.f.xxxx);
    if(SampleWeight > 0) {
        float3 Direct   = SampleCardAtlas3(g_CardAtlas_DirectIlluminationTexture, GatherPosition, Weights);
        float3 Indirect = SampleCardAtlas3(g_CardAtlas_IndirectIlluminationTexture, GatherPosition, Weights);
        float3 Lighting = SampleCardAtlas3(g_CardAtlas_LightingTexture, GatherPosition, Weights);
        float  Opacity  = SampleCardAtlas(g_CardAtlas_AlphaTexture, GatherPosition, Weights).r;

        Accumulator.DirectIllumination += Direct * Opacity;
        Accumulator.IndirectIllumination += Indirect * Opacity;
        Accumulator.Lighting += Lighting * Opacity;
        Accumulator.Opacity += Opacity * SampleWeight;
        Accumulator.WeightSum += SampleWeight;
    }
}

CardSampleAccumulator SampleCardSet (
    CardSet Cards,
    float3x4 WorldToInstanceTransform,
    float3x3 WorldToInstanceNormalTransform,
    float3 WorldPosition,
    float3 WorldViewDirection
) {
    float3 LocalPosition      = TransformPoint(WorldToInstanceTransform, WorldPosition);
    LocalPosition = clamp(LocalPosition, Cards.MinBounds, Cards.MaxBounds);
    float3 LocalViewDirection = normalize(TransformVector(WorldToInstanceNormalTransform, WorldViewDirection));
    // We can not attain accurate normals, so use the view direction as a proxy
    float3 ViewDirectionWeights = sqrt(abs(LocalViewDirection));
    ViewDirectionWeights = 1.f / dot(1.f.xxx, ViewDirectionWeights);
    // yz plane
    CardSampleAccumulator Accumulator = (CardSampleAccumulator)0;
    if(ViewDirectionWeights.x > UB.Card_MinCardViewDirectionWeightToSample) {
        int CardRank = GetCardRank(
            LocalPosition, Cards.NumCards, LocalViewDirection, Cards.MinBounds, Cards.MaxBounds, 0
        );
        int CardIndex = CardRank + Cards.CardIndexBase;
        Card CardHeader = FetchCard(CardIndex);
        CardSample Sample = SampleCard(
            CardIndex, CardHeader, Cards.CardResolutions.yz, Cards.MinBounds.yz, Cards.MaxBounds.yz, LocalPosition.yz
        );
        float HitDepth = LocalPosition.x - Cards.MinBounds.x;
        float InstanceZScale = Cards.MaxBounds.x - Cards.MinBounds.x;
        int   NumCards = Cards.NumCards.x / 2;
        AccumulateCardSample(Sample, HitDepth, InstanceZScale, NumCards, ViewDirectionWeights.x, Accumulator);
    }
    // zx plane
    if(ViewDirectionWeights.y > UB.Card_MinCardViewDirectionWeightToSample) {
        int CardRank = GetCardRank(
            LocalPosition, Cards.NumCards, LocalViewDirection, Cards.MinBounds, Cards.MaxBounds, 1
        );
        int CardIndex = CardRank + Cards.CardIndexBase;
        Card CardHeader = FetchCard(CardIndex);
        CardSample Sample = SampleCard( 
            CardIndex, CardHeader, Cards.CardResolutions.zx, Cards.MinBounds.zx, Cards.MaxBounds.zx, LocalPosition.zx
        );
        float HitDepth = LocalPosition.y - Cards.MinBounds.y;
        float InstanceZScale = Cards.MaxBounds.y - Cards.MinBounds.y;
        int   NumCards = Cards.NumCards.y / 2;
        AccumulateCardSample(Sample, HitDepth, InstanceZScale, NumCards, ViewDirectionWeights.y, Accumulator);
    }
    // xy plane
    if(ViewDirectionWeights.z > UB.Card_MinCardViewDirectionWeightToSample) {
        int CardRank = GetCardRank(
            LocalPosition, Cards.NumCards, LocalViewDirection, Cards.MinBounds, Cards.MaxBounds, 2
        );
        int CardIndex = CardRank + Cards.CardIndexBase;
        Card CardHeader = FetchCard(CardIndex);
        CardSample Sample = SampleCard(
            CardIndex, CardHeader, Cards.CardResolutions.xy, Cards.MinBounds.xy, Cards.MaxBounds.xy, LocalPosition.xy
        );
        float HitDepth = LocalPosition.z - Cards.MinBounds.z;
        float InstanceZScale = Cards.MaxBounds.z - Cards.MinBounds.z;
        int   NumCards = Cards.NumCards.z / 2;
        AccumulateCardSample(Sample, HitDepth, InstanceZScale, NumCards, ViewDirectionWeights.z, Accumulator);
    }
    return Accumulator;
}


#endif
#ifndef PROBES_HLSL
#define PROBES_HLSL

#include "3dgs_inc.hlsl"
#include "conventions.hlsl"
#include "sampling.hlsl"

// Probe headers
// Use textures for better texture cache utilization (2x2)
// BasisOffset : 24 bits
// ProbeClass  : 4  bits
// ProbeFlag   : 4  bits
// RWTexture2D<uint>   g_RWProbeHeaderPackedTexture;
RWTexture2D<uint>   g_RWProbeScreenCoordsTexture;
RWTexture2D<float>  g_RWProbeLinearDepthTexture;
RWTexture2D<float3> g_RWProbeWorldPositionTexture; 
RWTexture2D<uint>   g_RWProbeNormalTexture;
// RWTexture2D<uint>   g_RWPreviousProbeHeaderPackedTexture;
RWTexture2D<uint>   g_RWPreviousProbeScreenCoordsTexture;
RWTexture2D<float>  g_RWPreviousProbeLinearDepthTexture;
RWTexture2D<float4> g_RWPreviousProbeWorldPositionTexture;
RWTexture2D<uint>   g_RWPreviousProbeNormalTexture;
// Probe color maps
// R16G16B16A16, 8x8 per probe, 3 color + 1 linear depth
RWTexture2D<float4>  g_RWProbeColorTexture; 
RWTexture2D<float4>  g_RWPreviousProbeColorTexture;
// Padded probe color texture for hardware filtering
RWTexture2D<float4>  g_RWProbeSampleColorTexture;
Texture2D<float4>    g_ProbeSampleColorTexture;
// SH coefficients 8+1 per channel
RWTexture2D<float4>  g_RWProbeSHCoefficientsRTexture;
RWTexture2D<float4>  g_RWProbeSHCoefficientsGTexture;
RWTexture2D<float4>  g_RWProbeSHCoefficientsBTexture;
RWTexture2D<float4>  g_RWProbeIrradianceTexture;

// The estimated accuracy of the current probe from temporal reprojection
// [0, 1], used to guide update ratio
RWTexture2D<float>  g_RWProbeHistoryTrustTexture;

// Number of update rays allocated for each probe
// Must be a multiple of WAVE_SIZE
RWStructuredBuffer<uint>  g_RWProbeUpdateRayCountBuffer;
// Offset of update ray indices for each probe
RWStructuredBuffer<uint>  g_RWProbeUpdateRayOffsetBuffer;
// Total number of allocated update rays
RWStructuredBuffer<uint>  g_RWProbeAllUpdateRayCountBuffer;
// Index probe index with ray index (index with RayIndex / WAVE_SIZE as rays are allocated in waves)
RWStructuredBuffer<uint>  g_RWProbeUpdateRayProbeBuffer;
// Normal packed update ray direction
RWStructuredBuffer<uint>  g_RWProbeUpdateRayDirectionBuffer;
// bBypass & Traced Radiance & InvPdf for each update ray 
RWStructuredBuffer<uint2>  g_RWProbeUpdateRayResultBuffer;
// Hit depth for each update ray
RWStructuredBuffer<float>  g_RWProbeUpdateRayDepthBuffer;

// Number of ray hits that failed in reprojection and requires DI shading
RWStructuredBuffer<uint>  g_RWProbeUpdateRayHitShadeCountBuffer;
RWStructuredBuffer<uint>  g_RWProbeUpdateRayHitShadeListBuffer;

// Some probe update rays should resolve radiance results from the hash grid cache
// Record the cache cell index they resolve result from
RWStructuredBuffer<uint>  g_RWProbeUpdateRayResolveHashCellIndexBuffer;

// Extra info for secondary vertex shading
RWStructuredBuffer<float3>  g_RWProbeUpdateRayHitShadePositionBuffer;
RWStructuredBuffer<uint>    g_RWProbeUpdateRayHitShadeViewDirectionBuffer;

struct ProbeUpdateRayResult {
    // Whether we should bypass radiance cache for this ray.
    // Hits that can be projected onto the previous frame are bypassed.
    // Hits that are likely to cause hash grid light leaking are also bypassed.
    bool bBypass;
    float InvPdf;
    float3 Radiance;
};

ProbeUpdateRayResult FetchProbeUpdateRayResult (int RayIndex) {
    uint2 Packed = g_RWProbeUpdateRayResultBuffer[RayIndex];
    ProbeUpdateRayResult Result = (ProbeUpdateRayResult)0;
    Result.bBypass = (Packed.x & 0x80000000u) != 0;
    Packed.x &= 0x7fffffffu;
    float4 RadianceInvPdf = UnpackFp16x4(Packed);
    Result.InvPdf = RadianceInvPdf.w;
    Result.Radiance = RadianceInvPdf.xyz;
    return Result;
}

void WriteProbeUpdateRayResult (int RayIndex, ProbeUpdateRayResult Result) {
    uint2 Packed = PackFp16x4Safe(float4(Result.Radiance, Result.InvPdf));
    if(Result.bBypass) Packed.x |= 0x80000000u;
    g_RWProbeUpdateRayResultBuffer[RayIndex] = Packed;
}

// Radiance from direct emitters for each update ray
// RWStructuredBuffer<uint2>  g_RWUpdateRayRadianceEBuffer;
// RWStructuredBuffer<float>  g_RWUpdateRayLinearDepthBuffer;

// Number of adaptive probes within each tile
RWTexture2D<uint>          g_RWTileAdaptiveProbeCountTexture;
RWTexture2D<uint>          g_RWPreviousTileAdaptiveProbeCountTexture;
RWTexture2D<uint>          g_RWNextTileAdaptiveProbeCountTexture;
// Adaptive probe indices for each tile. The indexing rules are the same as Lumen.
RWTexture2D<uint>          g_RWTileAdaptiveProbeIndexTexture;
RWTexture2D<uint>          g_RWPreviousTileAdaptiveProbeIndexTexture;
// Count of adaptive probes allocated for this frame.
RWStructuredBuffer<uint>   g_RWAdaptiveProbeCountBuffer;

struct ProbeHeader {
    // Screen pixel coords of the probe
    int2 ScreenCoords;
    bool bValid;
    float  LinearDepth;
    float3 Position;
    float3 Normal;
};  

struct SSRC_SampleData {
    // Base atlas coords
    int2 Index[4];
    // Interpolation weights
    float4 Weights;
};

float3 GetScreenProbePosition (int2 ProbeIndex, bool bPrevious = false) {
    return bPrevious ? g_RWPreviousProbeWorldPositionTexture[ProbeIndex].xyz
        : g_RWProbeWorldPositionTexture[ProbeIndex].xyz;
}

float GetScreenProbeLinearDepth (int2 ProbeIndex, bool bPrevious = false) {
    return bPrevious ? g_RWPreviousProbeLinearDepthTexture[ProbeIndex].x
        : g_RWProbeLinearDepthTexture[ProbeIndex].x;
}

float3 GetScreenProbeNormal (int2 ProbeIndex, bool bPrevious = false) {
    return bPrevious
        ? OctahedronToUnitVector(UnpackUnorm16x2(g_RWPreviousProbeNormalTexture[ProbeIndex].x) * 2.f - 1.f)
        : OctahedronToUnitVector(UnpackUnorm16x2(g_RWProbeNormalTexture[ProbeIndex].x) * 2.f - 1.f);
}

ProbeHeader GetScreenProbeHeader (int2 ProbeIndex, bool bPrevious = false) {
    ProbeHeader Header = (ProbeHeader)0;
    Header.ScreenCoords = UnpackUint16x2(
        bPrevious
        ? g_RWPreviousProbeScreenCoordsTexture[ProbeIndex].x
        : g_RWProbeScreenCoordsTexture[ProbeIndex].x
    );
    Header.LinearDepth  = GetScreenProbeLinearDepth(ProbeIndex, bPrevious);
    Header.Position     = GetScreenProbePosition(ProbeIndex, bPrevious);
    Header.Normal       = GetScreenProbeNormal(ProbeIndex, bPrevious);
    Header.bValid       = Header.LinearDepth > 0;
    return Header;
}

void WriteScreenProbeHeader (int2 ProbeIndex, ProbeHeader Header) {
    g_RWProbeScreenCoordsTexture[ProbeIndex] = PackUint16x2(Header.ScreenCoords);
    g_RWProbeLinearDepthTexture[ProbeIndex] = Header.LinearDepth;
    g_RWProbeWorldPositionTexture[ProbeIndex] = Header.Position;
    g_RWProbeNormalTexture[ProbeIndex] = PackUnorm16x2(UnitVectorToOctahedron(Header.Normal) * 0.5f + 0.5f);
}

int2 GetProbeUpdateRayProbeIndex (int RayIndex) {
    return UnpackUint16x2(g_RWProbeUpdateRayProbeBuffer[RayIndex / WAVE_SIZE]);
}

int2 GetTileJitter (bool bPrevious = false) {
    int JitterIndex = (bPrevious ? UB.SSRC_PreviousTileJitterFrameSeed : UB.SSRC_TileJitterFrameSeed) % 16;
#ifdef MIRROR_REPEAT_TILE_JITTER_SEQUENCE
    int FinalJitterIndex = JitterIndex >= 8 ? 15 - JitterIndex : JitterIndex;
#else
    int FinalJitterIndex = JitterIndex % 8;
#endif
    return Hammersley16(FinalJitterIndex, 8, 0) * TILE_SIZE;
}

int2 GetScreenProbeScreenCoords (int2 ProbeIndex, bool bPrevious = false) {
    int2 TileJitter = GetTileJitter(bPrevious);
    int2 UniformScreenProbeScreenCoords = ProbeIndex * TILE_SIZE + TileJitter;
    if(any(ProbeIndex >= UB.TileDimensions)) {
        ProbeHeader Header = GetScreenProbeHeader(ProbeIndex, bPrevious);
        UniformScreenProbeScreenCoords = Header.ScreenCoords;
    }
    return UniformScreenProbeScreenCoords;
}

// Get the coords of a probe within the adaptive probe index texture
int2 GetAdaptiveProbeIndexCoords (int2 TileCoords, int AdaptiveProbeListIndex) {
	int2 CoordsWithinTile = int2(
        AdaptiveProbeListIndex % TILE_SIZE,
        AdaptiveProbeListIndex / TILE_SIZE
    );
	return CoordsWithinTile * UB.TileDimensions + TileCoords;
}

int  GetAdaptiveProbeIndex (int2 TileCoords, int AdaptiveProbeListIndex, bool bPrevious = false) {
    int2 IndexCoords = GetAdaptiveProbeIndexCoords(TileCoords, AdaptiveProbeListIndex);
    return bPrevious ? g_RWPreviousTileAdaptiveProbeIndexTexture[IndexCoords].x
        : g_RWTileAdaptiveProbeIndexTexture[IndexCoords].x;
}

int2 GetUniformScreenProbeScreenCoords (int2 TileCoords, bool bPrevious = false) {
    return TileCoords * TILE_SIZE + GetTileJitter(bPrevious);
}

float2 GetUniformScreenProbeScreenUV (int2 TileCoords, bool bPrevious = false) {
    return (GetUniformScreenProbeScreenCoords(TileCoords, bPrevious) + 0.5) * UB.InvScreenDimensions;
}

struct ScreenProbeMaterial {
    float3 Position;
    float  Depth;
    float3 GeometryNormal;
    bool   bValid;
};

bool   IsScreenProbeValid (int2 Index) {
    return g_RWProbeLinearDepthTexture[Index].x > 0;
}

float GetBasisOrderWeight (int Order) {
    return 1.f / (1U << Order);
}

float4 GetScreenProbeOctahedronRadianceDepth (int2 ProbeIndex, int2 TexelCoords, bool bPrevious = false) {
    int2 Coords = ProbeIndex * SSRC_PROBE_TEXTURE_SIZE + TexelCoords;
    return bPrevious ? g_RWPreviousProbeColorTexture[Coords] : g_RWProbeColorTexture[Coords];
}

void WriteScreenProbeOctahedronRadianceDepth (int2 ProbeIndex, int2 TexelCoords, float4 RadianceDepth, bool bPrevious = false) {
    int2 Coords = ProbeIndex * SSRC_PROBE_TEXTURE_SIZE + TexelCoords;
    if(!bPrevious) g_RWProbeColorTexture[Coords] = RadianceDepth;
    else g_RWPreviousProbeColorTexture[Coords] = RadianceDepth;
}

int SSRC_GetTotalProbeCount () {
    int ProbeCount = UB.SSRC_UniformScreenProbeCount + g_RWAdaptiveProbeCountBuffer[0];
    return ProbeCount;
}

int SSRC_GetTotalUpdateRayCount () {
    return g_RWProbeUpdateRayOffsetBuffer[SSRC_GetTotalProbeCount()];
}

#endif // PROBES_HLSL
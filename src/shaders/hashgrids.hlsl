#ifndef HASHGRIDS_HLSL
#define HASHGRIDS_HLSL

#include "3dgs_inc.hlsl"

// Same as GI10 hash grid design, each bucket can keep several tiles
// each tile contains 8x8 cells

RWStructuredBuffer<uint> g_HashGrids_TileHashBuffer;
RWStructuredBuffer<uint> g_HashGrids_TileTimestampBuffer;
// Cell values (radiance) cached in hash grids
RWStructuredBuffer<uint2> g_HashGrids_HistoryCellValueBuffer;
RWStructuredBuffer<uint> g_HashGrids_CellValueBuffer;
// A list of tiles that should be updated this frame
RWStructuredBuffer<uint> g_HashGrids_UpdateTileCountBuffer;
RWStructuredBuffer<uint> g_HashGrids_UpdateTileListBuffer;
// A list of active tile indices
RWStructuredBuffer<uint> g_HashGrids_ActiveTileCountBuffer;
RWStructuredBuffer<uint> g_HashGrids_ActiveTileListBuffer;
RWStructuredBuffer<uint> g_HashGrids_HistoryActiveTileCountBuffer;
RWStructuredBuffer<uint> g_HashGrids_HistoryActiveTileListBuffer;

#define HASHGRIDCACHE_STEP_FACTOR 1e3f
#define HASHGRIDCACHE_SIZE_FACTOR 1e-3f

float HashGrids_GetCellSize (float3 WorldPosition) {
    float Distance = distance(UB.HashGrids_Center, WorldPosition);
    float CellSize = max(UB.HashGrids_InvCascadeRadius * Distance * UB.HashGrids_CellSize, UB.HashGrids_CellSize);
    uint  L2CellSize = uint(log2(CellSize));
    return exp2(L2CellSize);
}

struct HashGrids_CellIndex {
    uint BucketIndex;
    uint TileHash;
    uint2 CellIndex;
};

HashGrids_CellIndex HashGrids_GetCell (float3 WorldPosition, float3 ViewDirection) {
    
}



#endif // HASHGRIDS_HLSL
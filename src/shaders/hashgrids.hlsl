#ifndef HASHGRIDS_HLSL
#define HASHGRIDS_HLSL

#include "3dgs_inc.hlsl"
#include "hash.hlsl"
#include "conventions.hlsl"

// Same as GI10 hash grid design, each bucket can keep several tiles
// each tile contains 8x8 cells

RWStructuredBuffer<int>  g_HashGrids_FreeTileCountBuffer;
RWStructuredBuffer<int>  g_HashGrids_FreeTileListBuffer;

RWStructuredBuffer<uint> g_HashGrids_BucketHashBuffer;
RWStructuredBuffer<uint> g_HashGrids_BucketTileIndexBuffer; // Store indices of tiles in the bucket

RWStructuredBuffer<uint> g_HashGrids_TileTimestampBuffer;
RWStructuredBuffer<uint> g_HashGrids_TileBucketHashBuffer;
// Cell values (radiance) cached in hash grids
RWStructuredBuffer<uint> g_HashGrids_CellValueBuffer; // 2 elements per cell
// This is quantilized and atomic accumulated, and only contains mip0 cells
RWStructuredBuffer<uint> g_HashGrids_UpdateCellValueXBuffer; // 4 elements per cell
// A list of tiles that should be updated this frame
RWStructuredBuffer<uint> g_HashGrids_UpdateTileCountBuffer;
RWStructuredBuffer<uint> g_HashGrids_UpdateTileListBuffer;
// A list of active tile indices
// RWStructuredBuffer<uint> g_HashGrids_AllocatedTileCountBuffer;
// RWStructuredBuffer<uint> g_HashGrids_AllocatedTileListBuffer;
RWStructuredBuffer<uint> g_HashGrids_ActiveTileCountBeforeAllocationBuffer;
RWStructuredBuffer<uint> g_HashGrids_ActiveTileCountBuffer;
RWStructuredBuffer<uint> g_HashGrids_ActiveTileListBuffer;
RWStructuredBuffer<uint> g_HashGrids_HistoryActiveTileCountBuffer;
RWStructuredBuffer<uint> g_HashGrids_HistoryActiveTileListBuffer;

#define HASHGRIDS_RADIANCE_QUANTILIZATION_MULTIPLIER 2048.f

uint4 HashGrids_QuantilizeRadianceSampleCount (float4 RadianceW) {
    return uint4(
        uint(round(RadianceW.x * HASHGRIDS_RADIANCE_QUANTILIZATION_MULTIPLIER)),
        uint(round(RadianceW.y * HASHGRIDS_RADIANCE_QUANTILIZATION_MULTIPLIER)),
        uint(round(RadianceW.z * HASHGRIDS_RADIANCE_QUANTILIZATION_MULTIPLIER)),
        uint(RadianceW.w)
    );
}

float4 HashGrids_RecoverRadianceSampleCount (uint4 QuantilizedRadiance) {
    return float4(
        QuantilizedRadiance.x / HASHGRIDS_RADIANCE_QUANTILIZATION_MULTIPLIER,
        QuantilizedRadiance.y / HASHGRIDS_RADIANCE_QUANTILIZATION_MULTIPLIER,
        QuantilizedRadiance.z / HASHGRIDS_RADIANCE_QUANTILIZATION_MULTIPLIER,
        QuantilizedRadiance.w
    );
}

float HashGrids_GetCellSize (float3 WorldPosition) {
    float Distance = distance(UB.HashGrids_Center, WorldPosition);
    float CellSize = max(UB.HashGrids_InvCascadeRadius * Distance * UB.HashGrids_CellSize, UB.HashGrids_CellSize);
    int   L2CellSize = int(floor(log2(CellSize)));
    return exp2(L2CellSize);
}

struct HashGridsKey {
    uint BucketHash;
    uint2 CellOffset;
};

HashGridsKey HashGrids_GetEntryKey (float3 WorldPosition, float3 ViewDirection, float TraveledDistance = 0) {
    // Get rid of artifacts along axis-aligned planes that are exactly multiple of tile sizes (due to hash grid quantization)
    WorldPosition += float3(0.00007893f, 0.00008461f, 0.00002847f);
    float CellSize = HashGrids_GetCellSize(WorldPosition);
    float TileSize = HASHGRIDS_TILE_CELL_WIDTH * CellSize;
    int3  TileIndex = floor(WorldPosition / TileSize);
    // Geometries smaller than a tile may have different radiance responses for queries
    // come from outside of the tile and inside of the tile, so we hash them separately
    // Imagine a box small enough to fit in a tile, light queries from within the box
    // should get different values than queries from outside of the box
    // TODO: seems unworthy...
    bool bWithinTile = false;//TraveledDistance < TileSize;
    uint4 Features0 = uint4(asuint(TileIndex), uint(max(0, 100 + floor(log2(CellSize)))) + (bWithinTile ? 200 : 0));
    float3 QuantilizedViewDirection = floor(0.5f + 4 * (ViewDirection * 0.5 + 0.5));
    uint3 Features1 = QuantilizedViewDirection;
    uint BucketHash = pcgHash(uint4(Features1, pcgHash(Features0)));
    BucketHash = max(BucketHash, 1); // 0 is reserved for empty tile marker
    float3 CellOffset3 = WorldPosition / CellSize - TileIndex * HASHGRIDS_TILE_CELL_WIDTH;
    // Pick a plane with the most normal component
    float3 Normal = abs(ViewDirection);
    float  MaxComponent = max(Normal.x, max(Normal.y, Normal.z));
    uint2 CellOffset;
    if(Normal.x == MaxComponent) {
        CellOffset = uint2(CellOffset3.y, CellOffset3.z);
    } else if(Normal.y == MaxComponent) {
        CellOffset = uint2(CellOffset3.x, CellOffset3.z);
    } else {
        CellOffset = uint2(CellOffset3.x, CellOffset3.y);
    }
    HashGridsKey Result = (HashGridsKey)0;
    Result.BucketHash  = BucketHash;
    Result.CellOffset = CellOffset;
    return Result;
}

uint HashGrids_GetCellIndex (uint TileIndex, uint2 CellOffset, uint MipLevel = 0) {
    uint Offsets[4] = {
        HASHGRIDS_TILE_CELL_MIP_OFFSET_0,
        HASHGRIDS_TILE_CELL_MIP_OFFSET_1,
        HASHGRIDS_TILE_CELL_MIP_OFFSET_2,
        HASHGRIDS_TILE_CELL_MIP_OFFSET_3
    };
    return TileIndex * HASHGRIDS_NUM_CELLS_PER_TILE + Offsets[MipLevel] + CellOffset.x + CellOffset.y * (uint(HASHGRIDS_TILE_CELL_WIDTH) >> MipLevel);
}

// Find and (if not found) allocate a slot in the hash table
// Return the slot index in the hash table.
uint HashGrids_FindAndAllocate (uint BucketHash, out bool bIsNewSlot) {
    uint BucketIndex = BucketHash % UB.HashGrids_NumBuckets;
    int TileRank = 0, BucketSlotIndex = 0;
    uint PrevBucketHash = 0;
    [unroll(HASHGRIDS_MAX_NUM_ENTRIES_SEARCHED_PER_BUCKET)]
    for(; TileRank < UB.HashGrids_MaxNumEntriesSearchedPerBucket; TileRank ++) {
        BucketSlotIndex = BucketIndex * UB.HashGrids_NumInterleavedEntriesPerBucket + TileRank;
        // Try to allocate a tile (if it is empty)
        InterlockedCompareExchange(g_HashGrids_BucketHashBuffer[BucketSlotIndex], 0, BucketHash, PrevBucketHash);
        if(PrevBucketHash == 0) {
            break; // Allocated a new tile
        }
        if(PrevBucketHash == BucketHash) {
            break; // Found existing tile
        }
    }
    bIsNewSlot = PrevBucketHash == 0;
    if(TileRank == UB.HashGrids_MaxNumEntriesSearchedPerBucket) {
        return INVALID_U32; // No more space in the bucket
    }
    return BucketSlotIndex;
}

// Find a slot in the hash table
// Return the slot index in the hash table.
uint HashGrids_Find (uint BucketHash) {
    uint BucketIndex = BucketHash % UB.HashGrids_NumBuckets;
    int TileRank = 0, BucketSlotIndex = 0;
    uint PrevBucketHash = 0;
    [unroll(HASHGRIDS_MAX_NUM_ENTRIES_SEARCHED_PER_BUCKET)]
    for(; TileRank < UB.HashGrids_MaxNumEntriesSearchedPerBucket; TileRank ++) {
        BucketSlotIndex = BucketIndex * UB.HashGrids_NumInterleavedEntriesPerBucket + TileRank;
        PrevBucketHash = g_HashGrids_BucketHashBuffer[BucketSlotIndex];
        if(PrevBucketHash == BucketHash) {
            break; // Found existing tile
        }
        if(PrevBucketHash == 0) {
            break; // Not found, terminate the search
        }
    }
    return (PrevBucketHash == BucketHash) ? BucketSlotIndex : INVALID_U32;
}

// Return the cell index to index int the value buffer
// ViewDirection is the view direction "watching" the cell
uint HashGrids_AllocateTile (float3 WorldPosition, float3 ViewDirection, inout uint BucketSlotIndex, out uint2 CellOffset) {
    HashGridsKey Key = HashGrids_GetEntryKey(WorldPosition, ViewDirection);
    bool bIsNewSlot = false;
    BucketSlotIndex = HashGrids_FindAndAllocate(Key.BucketHash, bIsNewSlot);
    if(BucketSlotIndex == INVALID_U32) return INVALID_U32;
    uint TileIndex = INVALID_U32;
    if(bIsNewSlot) {
        // No previous tile found, allocate a new one
        int TileFreeListIndex = 0;
        InterlockedAdd(g_HashGrids_FreeTileCountBuffer[0], -1, TileFreeListIndex);
        TileFreeListIndex --;
        if(TileFreeListIndex >= 0) {
            TileIndex = g_HashGrids_FreeTileListBuffer[TileFreeListIndex];
            // Register the tile to the active list
            uint ActiveListIndex;
            InterlockedAdd(g_HashGrids_ActiveTileCountBuffer[0], 1, ActiveListIndex);
            g_HashGrids_ActiveTileListBuffer[ActiveListIndex] = TileIndex;
            // Keep the key to index the tile for re-insertion
            g_HashGrids_TileBucketHashBuffer[TileIndex] = Key.BucketHash;
            // Record the mapping from bucket slot to tile index for future lookups in this frame
            g_HashGrids_BucketTileIndexBuffer[BucketSlotIndex] = TileIndex;
        }
    }
    if (IsValid(TileIndex)) {
        uint Timestamp = UB.FrameIndex + 1, PrevTimestamp = 0;
        InterlockedExchange(g_HashGrids_TileTimestampBuffer[TileIndex], Timestamp, PrevTimestamp);
        // This tile is touched (for the first time in this frame), queue it up for update.
        int UpdateListIndex = 0;
        InterlockedAdd(g_HashGrids_UpdateTileCountBuffer[0], 1, UpdateListIndex);
        g_HashGrids_UpdateTileListBuffer[UpdateListIndex] = TileIndex;
    }
    return TileIndex;
}

float4 HashGrids_GetCellRadiance (uint CellIndex) {
    return UnpackFp16x4(uint2(g_HashGrids_CellValueBuffer[CellIndex * 2 + 0],
                              g_HashGrids_CellValueBuffer[CellIndex * 2 + 1]));
}

float4 HashGrids_GetUpdateCellRadiance (uint CompactCellIndex) {
    return 
        HashGrids_RecoverRadianceSampleCount(uint4(
            g_HashGrids_UpdateCellValueXBuffer[CompactCellIndex * 4 + 0],
            g_HashGrids_UpdateCellValueXBuffer[CompactCellIndex * 4 + 1],
            g_HashGrids_UpdateCellValueXBuffer[CompactCellIndex * 4 + 2],
            g_HashGrids_UpdateCellValueXBuffer[CompactCellIndex * 4 + 3]
        ));
}

float4 HashGrids_GetFilteredRadiance(uint CellIndexMip0)
{
    uint TileIndex = CellIndexMip0 / HASHGRIDS_NUM_CELLS_PER_TILE;
    uint CellRank = CellIndexMip0 % HASHGRIDS_NUM_CELLS_PER_TILE;
    uint2 CellOffset = uint2(CellRank % HASHGRIDS_TILE_CELL_WIDTH, CellRank / HASHGRIDS_TILE_CELL_WIDTH);
    uint CellIndexMip1 = HashGrids_GetCellIndex(TileIndex, CellOffset / 2, 1);
    uint CellIndexMip2 = HashGrids_GetCellIndex(TileIndex, CellOffset / 4, 2);
    uint CellIndexMip3 = HashGrids_GetCellIndex(TileIndex, CellOffset / 8, 3);
    // Select best mip
    float4 Radiance;

    // Mip 0
    Radiance = HashGrids_GetCellRadiance(CellIndexMip0);
    
    // Mip 1
    Radiance = Radiance.w < UB.HashGrids_TargetSampleCount ?
         HashGrids_GetCellRadiance(CellIndexMip1) : Radiance;
    
    // Mip 2
    Radiance = Radiance.w < UB.HashGrids_TargetSampleCount ?
        HashGrids_GetCellRadiance(CellIndexMip2) : Radiance;
    
    // Mip 3
    Radiance = Radiance.w < UB.HashGrids_TargetSampleCount ?
        HashGrids_GetCellRadiance(CellIndexMip3) : Radiance;

    return Radiance;
}

uint HashGrids_CellIndexToCompactCellIndex (uint CellIndex) {
    uint TileIndex = CellIndex / HASHGRIDS_NUM_CELLS_PER_TILE;
    uint CellRank  = CellIndex % HASHGRIDS_NUM_CELLS_PER_TILE;
    return TileIndex * HASHGRIDS_TILE_CELL_MIP_OFFSET_1 + CellRank;
}


void HashGrids_AccumulateSamplesToCell (uint CompactCellIndex, float3 Radiance, uint SampleCount) {
    uint4 QuantilizedRadiance = HashGrids_QuantilizeRadianceSampleCount(float4(Radiance, SampleCount));
    if(dot(Radiance, 1.f.xxx) > 0) {
        InterlockedAdd(g_HashGrids_UpdateCellValueXBuffer[CompactCellIndex * 4 + 0], QuantilizedRadiance.x);
        InterlockedAdd(g_HashGrids_UpdateCellValueXBuffer[CompactCellIndex * 4 + 1], QuantilizedRadiance.y);
        InterlockedAdd(g_HashGrids_UpdateCellValueXBuffer[CompactCellIndex * 4 + 2], QuantilizedRadiance.z);
    }
    InterlockedAdd(g_HashGrids_UpdateCellValueXBuffer[CompactCellIndex * 4 + 3], QuantilizedRadiance.w);
}








#endif // HASHGRIDS_HLSL
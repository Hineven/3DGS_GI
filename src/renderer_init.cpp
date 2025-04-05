/*
 * Created: 2024/11/14
 * Author:  hineven
 * See LICENSE for licensing.
 */
#include "renderer.h"
#include "3dgs_shared.hlsl"
#include "glm/gtc/round.hpp"


bool Renderer::CreateResources () {
    // a maximum of 12 million gaussians
    int max_num_gaussians = 1024 * 1024 * 12;
    int max_num_gaussian_instances = 1024 * 1024 * 8 * 8;
    int width = AppInternal::GetInstance().GetWindowWidth();
    int height = AppInternal::GetInstance().GetWindowHeight();
    auto & gfx = AppInternal::GetInstance().GetGfx();
    buf_.dispatch_indirect_command = gfxCreateBuffer<DispatchIndirectCommand>(gfx, 1);
    buf_.dispatch_indirect_command.setName("DispatchIndirectCommand");
    buf_.dispatch_rays_indirect_command = gfxCreateBuffer<DispatchRaysIndirectCommand>(gfx, 1);
    buf_.dispatch_rays_indirect_command.setName("DispatchRaysIndirectCommand");
    buf_.draw_indirect_command = gfxCreateBuffer<DrawIndirectCommand>(gfx, 1);
    buf_.draw_indirect_command.setName("DrawIndirectCommand");

    buf_.probe_dispatch_command = gfxCreateBuffer<DispatchIndirectCommand>(gfx, 1);
    buf_.probe_dispatch_command.setName("ProbeDispatchCommand");
    buf_.probe_per_lane_dispatch_command = gfxCreateBuffer<DispatchIndirectCommand>(gfx, 1);
    buf_.probe_per_lane_dispatch_command.setName("ProbePerLaneDispatchCommand");

    buf_.probe_update_ray_reduce_count = gfxCreateBuffer<uint>(gfx, 1);
    buf_.probe_update_ray_reduce_count.setName("ProbeUpdateRayReduceCount");

    buf_.LightGrid_grid_light_list_allocator = gfxCreateBuffer<uint>(gfx, 1);
    buf_.LightGrid_grid_light_list_allocator.setName("LightGridGridLightListAllocator");
    int num_light_grids = options_.light_grid_size * options_.light_grid_size * options_.light_grid_size * options_.light_grid_num_cascades;
    buf_.LightGrid_grid_light_count = gfxCreateBuffer<uint>(gfx, num_light_grids);
    buf_.LightGrid_grid_light_count.setName("LightGridGridLightCount");
    buf_.LightGrid_grid_light_list_offset = gfxCreateBuffer<uint>(gfx, num_light_grids);
    buf_.LightGrid_grid_light_list_offset.setName("LightGridGridLightListOffset");
    buf_.LightGrid_grid_light_list = gfxCreateBuffer<uint>(gfx, options_.light_grid_max_num_entries);
    buf_.LightGrid_grid_light_list.setName("LightGridGridLightList");
    buf_.LightGrid_grid_reservoir_weight = gfxCreateBuffer<float>(gfx, options_.light_grid_max_num_entries);
    buf_.LightGrid_grid_reservoir_weight.setName("LightGridGridReservoirWeight");

    buf_.HashGrids_free_tile_count = gfxCreateBuffer<uint>(gfx, 1);
    buf_.HashGrids_free_tile_count.setName("HashGridsFreeTileCount");
    buf_.HashGrids_free_tile_list = gfxCreateBuffer<uint>(gfx, options_.HashGrids_max_num_tiles);
    buf_.HashGrids_free_tile_list.setName("HashGridsFreeTileList");
    buf_.HashGrids_bucket_hash = gfxCreateBuffer<uint>(gfx, options_.HashGrids_max_num_buckets * options_.HashGrids_num_slots_per_bucket);
    buf_.HashGrids_bucket_hash.setName("HashGridsBucketHash");
    buf_.HashGrids_bucket_tile_index = gfxCreateBuffer<uint>(gfx, options_.HashGrids_max_num_buckets * options_.HashGrids_num_slots_per_bucket);
    buf_.HashGrids_bucket_tile_index.setName("HashGridsBucketTileIndex");
    buf_.HashGrids_tile_timestamp = gfxCreateBuffer<uint>(gfx, options_.HashGrids_max_num_tiles);
    buf_.HashGrids_tile_timestamp.setName("HashGridsTileTimestamp");
    buf_.HashGrids_tile_bucket_hash = gfxCreateBuffer<uint>(gfx, options_.HashGrids_max_num_tiles);
    buf_.HashGrids_tile_bucket_hash.setName("HashGridsTileBucketHash");
    buf_.HashGrids_cell_value = gfxCreateBuffer<uint2>(gfx, options_.HashGrids_max_num_tiles * HASHGRIDS_NUM_CELLS_PER_TILE);
    buf_.HashGrids_cell_value.setName("HashGridsCellValue");
    buf_.HashGrids_update_cell_value_X = gfxCreateBuffer<uint4>(gfx, options_.HashGrids_max_num_tiles * HASHGRIDS_TILE_CELL_MIP_OFFSET_1);
    buf_.HashGrids_update_cell_value_X.setName("HashGridsUpdateCellValueX");
    buf_.HashGrids_update_tile_count = gfxCreateBuffer<uint>(gfx, 1);
    buf_.HashGrids_update_tile_count.setName("HashGridsUpdateTileCount");
    buf_.HashGrids_update_tile_list = gfxCreateBuffer<uint>(gfx, options_.HashGrids_max_num_tiles);
    buf_.HashGrids_update_tile_list.setName("HashGridsUpdateTileList");
    buf_.HashGrids_active_tile_count_before_allocation = gfxCreateBuffer<uint>(gfx, 1);
    buf_.HashGrids_active_tile_count_before_allocation.setName("HashGridsActiveTileCountBeforeAllocation");
    buf_.HashGrids_active_tile_count[0] = gfxCreateBuffer<uint>(gfx, 1);
    buf_.HashGrids_active_tile_count[0].setName("HashGridsActiveTileCount0");
    buf_.HashGrids_active_tile_count[1] = gfxCreateBuffer<uint>(gfx, 1);
    buf_.HashGrids_active_tile_count[1].setName("HashGridsActiveTileCount1");
    buf_.HashGrids_active_tile_list[0] = gfxCreateBuffer<uint>(gfx, options_.HashGrids_max_num_tiles);
    buf_.HashGrids_active_tile_list[0].setName("HashGridsActiveTileList");
    buf_.HashGrids_active_tile_list[1] = gfxCreateBuffer<uint>(gfx, options_.HashGrids_max_num_tiles);
    buf_.HashGrids_active_tile_list[1].setName("HashGridsActiveTileList");

    // Leave 1 extra slot for the "sum all" operation
    buf_.probe_update_ray_counts = gfxCreateBuffer<uint>(gfx, options_.SSRC_max_num_probes + 1);
    buf_.probe_update_ray_counts.setName("ProbeUpdateRayCounts");
    buf_.probe_update_ray_offsets = gfxCreateBuffer<uint>(gfx, options_.SSRC_max_num_probes + 1);
    buf_.probe_update_ray_offsets.setName("ProbeUpdateRayOffsets");
    buf_.probe_all_update_ray_count = gfxCreateBuffer<uint>(gfx, 1);
    buf_.probe_all_update_ray_count.setName("ProbeAllUpdateRayCount");
    buf_.probe_update_ray_probe = gfxCreateBuffer<uint>(gfx, divideAndRoundUp(options_.SSRC_max_num_probe_update_rays, cfg_.wave_lane_count));
    buf_.probe_update_ray_probe.setName("ProbeUpdateRayProbe");
    buf_.probe_update_ray_direction = gfxCreateBuffer<uint>(gfx, options_.SSRC_max_num_probe_update_rays);
    buf_.probe_update_ray_direction.setName("ProbeUpdateRayDirection");
    buf_.probe_update_ray_result = gfxCreateBuffer<uint2>(gfx, options_.SSRC_max_num_probe_update_rays);
    buf_.probe_update_ray_result.setName("ProbeUpdateRayResult");
    buf_.probe_update_ray_depth = gfxCreateBuffer<float>(gfx, options_.SSRC_max_num_probe_update_rays);
    buf_.probe_update_ray_hit_shade_count = gfxCreateBuffer<uint>(gfx, 1);
    buf_.probe_update_ray_hit_shade_count.setName("ProbeUpdateRayHitShadeCount");
    buf_.probe_update_ray_hit_shade_list = gfxCreateBuffer<uint>(gfx, options_.SSRC_max_num_probe_update_rays);
    buf_.probe_update_ray_hit_shade_list.setName("ProbeUpdateRayHitShadeList");
    buf_.probe_update_ray_resolve_hash_cell_index = gfxCreateBuffer<uint>(gfx, options_.SSRC_max_num_probe_update_rays);
    buf_.adaptive_probe_count = gfxCreateBuffer<uint>(gfx, 1);
    buf_.adaptive_probe_count.setName("AdaptiveProbeCount");

    buf_.active_gaussian_count = gfxCreateBuffer<int>(gfx, 1);
    buf_.active_gaussian_count.setName("GaussianActiveCount");
    buf_.active_gaussian_list = gfxCreateBuffer<int>(gfx, max_num_gaussians);
    buf_.active_gaussian_list.setName("ActiveGaussianList");
    buf_.active_gaussian_indirect = gfxCreateBuffer<int>(gfx, max_num_gaussians);
    buf_.active_gaussian_indirect.setName("ActiveGaussianIndirect");
    buf_.active_gaussian_indirect_src = gfxCreateBuffer<int>(gfx, max_num_gaussians);
    buf_.active_gaussian_indirect_src.setName("ActiveGaussianIndirectSrc");
    buf_.active_gaussian_linear_depth_src = gfxCreateBuffer<float>(gfx, max_num_gaussians);
    buf_.active_gaussian_linear_depth_src.setName("ActiveGaussianSrcDepth");
    buf_.active_gaussian_linear_depth = gfxCreateBuffer<float>(gfx, max_num_gaussians);
    buf_.active_gaussian_linear_depth.setName("ActiveGaussianDepth");

    // 2x16  packed
    buf_.active_gaussian_NDC_position = gfxCreateBuffer<uint>(gfx, max_num_gaussians);
    buf_.active_gaussian_NDC_position.setName("ActiveGaussianNDCPosition");
    // 2x16  packed
    buf_.active_gaussian_quad_NDC_vector0 = gfxCreateBuffer<uint>(gfx, max_num_gaussians);
    buf_.active_gaussian_quad_NDC_vector0.setName("ActiveGaussianQuadNDCVector0");
    // 2x16  packed
    buf_.active_gaussian_quad_NDC_vector1 = gfxCreateBuffer<uint>(gfx, max_num_gaussians);
    buf_.active_gaussian_quad_NDC_vector1.setName("ActiveGaussianQuadNDCVector1");

    buf_.active_gaussian_color = gfxCreateBuffer<glm::vec3>(gfx, max_num_gaussians);
    buf_.active_gaussian_color.setName("ActiveGaussianColor");

    int num_tiles = divideAndRoundUp(width, TILE_SIZE) * divideAndRoundUp(height, TILE_SIZE);


    int max_num_rays = options_.max_num_rays;

    buf_.ray_count = gfxCreateBuffer<int>(gfx, 1);
    buf_.ray_count.setName("RayCount");
    buf_.ray_to_trace_count[0] = gfxCreateBuffer<int>(gfx, 1);
    buf_.ray_to_trace_count[0].setName("RayToTraceCount0");
    buf_.ray_to_trace_count[1] = gfxCreateBuffer<int>(gfx, 1);
    buf_.ray_to_trace_count[1].setName("RayToTraceCount1");
    buf_.ray_to_trace_list[0] = gfxCreateBuffer<uint>(gfx, max_num_rays);
    buf_.ray_to_trace_list[0].setName("RayToTraceList0");
    buf_.ray_to_trace_list[1] = gfxCreateBuffer<uint>(gfx, max_num_rays);
    buf_.ray_to_trace_list[1].setName("RayToTraceList1");
    buf_.ray_to_trace_direction = gfxCreateBuffer<uint>(gfx, max_num_rays);
    buf_.ray_to_trace_direction.setName("RayToTraceDirection");
    buf_.ray_to_trace_origin = gfxCreateBuffer<glm::vec3>(gfx, max_num_rays);
    buf_.ray_to_trace_origin.setName("RayToTraceOrigin");
    buf_.ray_to_trace_UV_position = gfxCreateBuffer<uint>(gfx, max_num_rays);
    buf_.ray_to_trace_UV_position.setName("RayToTraceUVPosition");
    buf_.ray_to_trace_seed = gfxCreateBuffer<float>(gfx, max_num_rays);
    buf_.ray_to_trace_seed.setName("RayToTraceSeed");
    buf_.ray_to_trace_flags = gfxCreateBuffer<uint>(gfx, max_num_rays);
    buf_.ray_to_trace_flags.setName("RayToTraceFlags");

    buf_.ray_to_trace_result = gfxCreateBuffer<uint2>(gfx, max_num_rays);
    buf_.ray_to_trace_result.setName("RayToTraceResult");

    int max_num_pixels = width * height;
    buf_.direct_illumination_ray_occlusion_threshold = gfxCreateBuffer<float>(gfx, std::max(max_num_pixels, options_.SSRC_max_num_probe_update_rays));
    buf_.direct_illumination_ray_occlusion_threshold.setName("DirectIlluminationRayOcclusionThreshold");
    buf_.direct_illumination_ray_contribution = gfxCreateBuffer<uint2>(gfx, std::max(max_num_pixels, options_.SSRC_max_num_probe_update_rays));
    buf_.direct_illumination_ray_contribution.setName("DirectIlluminationRayContribution");
    buf_.direct_illumination_ray_probe_update_ray_index = gfxCreateBuffer(gfx, options_.SSRC_max_num_probe_update_rays);
    buf_.direct_illumination_ray_probe_update_ray_index.setName("DirectIlluminationRayProbeUpdateRayIndex");

    assert(max_num_rays >= options_.SSRC_max_num_probe_update_rays);
    assert(max_num_rays >= max_num_pixels);

    buf_.card_sets = gfxCreateBuffer<uint2>(gfx, cfg_.max_num_instances);
    buf_.card_sets.setName("CardSets");
    buf_.cards = gfxCreateBuffer<uint>(gfx, CARD_ATLAS_RESOLUTION  * CARD_ATLAS_RESOLUTION * NUM_CARD_ATLAS / MIN_CARD_RESOLUTION / MIN_CARD_RESOLUTION);
    buf_.cards.setName("Cards");

#ifndef NDEBUG
    buf_.Debug_SSRC_probe_index = gfxCreateBuffer<uint2>(gfx, 1);

    buf_.Debug_direct_illumination_pixel_ray_index = gfxCreateBuffer<uint>(gfx, max_num_pixels);
    buf_.Debug_direct_illumination_pixel_ray_index.setName("Debug_DirectIlluminationPixelRayIndex");
    buf_.Debug_visualize_ray_count = gfxCreateBuffer<int>(gfx, 1);
    buf_.Debug_visualize_ray_count.setName("Debug_VisualizeRayCount");
    buf_.Debug_visualize_ray_vertex = gfxCreateBuffer<glm::vec3>(gfx, 512);
    buf_.Debug_visualize_ray_vertex.setName("Debug_VisualizeRayVertex");
    buf_.Debug_visualize_ray_color = gfxCreateBuffer<glm::vec3>(gfx, 512);
    buf_.Debug_visualize_ray_color.setName("Debug_VisualizeRayColor");
    buf_.Debug_visualize_ray_ray_index = gfxCreateBuffer<int>(gfx, 512);
    buf_.Debug_visualize_ray_ray_index.setName("Debug_VisualizeRayRayIndex");
#endif

    uint UB_stride = roundUp((uint32_t)sizeof(UniformBlock), 256u);
    buf_.UB_pool = gfxCreateBuffer(gfx, cfg_.uniform_buffer_size, nullptr, kGfxCpuAccess_Write);
    buf_.UB_pool.setName("UniformBufferPool");
    buf_.UB_pool.setStride(UB_stride);

    float zero_clear_value[4] = {0, 0, 0, 0};
    tex_.G_depth = gfxCreateTexture2D(gfx, width, height, options_.depth_format, 1, zero_clear_value);
    tex_.G_depth.setName("G_depth");
    tex_.G_albedo_alpha = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 1, zero_clear_value);
    tex_.G_albedo_alpha.setName("G_albedo_alpha");
    tex_.G_material = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8_UNORM, 1, zero_clear_value);
    tex_.G_material.setName("G_material");
    tex_.G_normal[0] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 1, zero_clear_value);
    tex_.G_normal[0].setName("G_normal0");
    tex_.G_normal[1] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 1, zero_clear_value);
    tex_.G_normal[1].setName("G_normal1");
    tex_.G_gaussian_normal = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 1, zero_clear_value);
    tex_.G_gaussian_normal.setName("G_gaussian_normal");
    tex_.G_emission_alpha = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.G_emission_alpha.setName("G_emission_alpha");
    tex_.G_zdepth[0] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R32_FLOAT, 1, zero_clear_value);
    tex_.G_zdepth[0].setName("G_zdepth0");
    tex_.G_zdepth[1] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R32_FLOAT, 1, zero_clear_value);
    tex_.G_zdepth[1].setName("G_zdepth1");
    tex_.G_filtered_depth = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R32_FLOAT, 1, zero_clear_value);
    tex_.G_filtered_depth.setName("G_filtered_depth");
    int num_mips = gfxCalculateMipCount(width, height);
    assert(num_mips > 1);
    {
        // Round up to nearest power of 2 to build a "complete tree" mip chain (which is required for HZB)
        int W = glm::ceilPowerOfTwo(width) / 2;
        int H = glm::ceilPowerOfTwo(height) / 2;
        W = glm::max(W, H);
        int num_HZB_mips = gfxCalculateMipCount(W, W);
        tex_.near_HZB = gfxCreateTexture2D(gfx, W, W, DXGI_FORMAT_R32_FLOAT, num_HZB_mips, zero_clear_value);
        tex_.near_HZB.setName("NearHZB");
    }
    tex_.debug = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.debug.setName("Debug");

    int probe_width = divideAndRoundUp(width, TILE_SIZE);
    int probe_height = divideAndRoundUp(options_.SSRC_max_num_probes, probe_width);
    tex_.probe_screen_coords[0] = gfxCreateTexture2D(gfx, probe_width, probe_height, DXGI_FORMAT_R32_UINT);
    tex_.probe_screen_coords[0].setName("ProbeScreenCoords0");
    tex_.probe_screen_coords[1] = gfxCreateTexture2D(gfx, probe_width, probe_height, DXGI_FORMAT_R32_UINT);
    tex_.probe_screen_coords[1].setName("ProbeScreenCoords1");
    tex_.probe_linear_depth[0] = gfxCreateTexture2D(gfx, probe_width, probe_height, DXGI_FORMAT_R32_FLOAT);
    tex_.probe_linear_depth[0].setName("ProbeLinearDepth0");
    tex_.probe_linear_depth[1] = gfxCreateTexture2D(gfx, probe_width, probe_height, DXGI_FORMAT_R32_FLOAT);
    tex_.probe_linear_depth[1].setName("ProbeLinearDepth1");
    tex_.probe_world_position[0] = gfxCreateTexture2D(gfx, probe_width, probe_height, DXGI_FORMAT_R32G32B32A32_FLOAT);
    tex_.probe_world_position[0].setName("ProbeWorldPosition0");
    tex_.probe_world_position[1] = gfxCreateTexture2D(gfx, probe_width, probe_height, DXGI_FORMAT_R32G32B32A32_FLOAT);
    tex_.probe_world_position[1].setName("ProbeWorldPosition1");
    tex_.probe_normal[0] = gfxCreateTexture2D(gfx, probe_width, probe_height, DXGI_FORMAT_R32_UINT);
    tex_.probe_normal[0].setName("ProbeNormal0");
    tex_.probe_normal[1] = gfxCreateTexture2D(gfx, probe_width, probe_height, DXGI_FORMAT_R32_UINT);
    tex_.probe_normal[1].setName("ProbeNormal1");
    int probe_atlas_width = probe_width * SSRC_PROBE_TEXTURE_SIZE;
    int probe_atlas_height = probe_height * SSRC_PROBE_TEXTURE_SIZE;
    tex_.probe_color[0] = gfxCreateTexture2D(gfx, probe_atlas_width, probe_atlas_height, DXGI_FORMAT_R16G16B16A16_FLOAT);
    tex_.probe_color[0].setName("ProbeColor0");
    tex_.probe_color[1] = gfxCreateTexture2D(gfx, probe_atlas_width, probe_atlas_height, DXGI_FORMAT_R16G16B16A16_FLOAT);
    tex_.probe_color[1].setName("ProbeColor1");
    int probe_atlas_width_ex = probe_width * (1 + SSRC_PROBE_TEXTURE_SIZE + 1);
    int probe_atlas_height_ex = probe_height * (1 + SSRC_PROBE_TEXTURE_SIZE + 1);
    tex_.probe_sample_color = gfxCreateTexture2D(gfx, probe_atlas_width_ex, probe_atlas_height_ex, DXGI_FORMAT_R16G16B16A16_FLOAT);
    tex_.probe_sample_color.setName("ProbeSampleColor");
    tex_.probe_SH_coefficients_R = gfxCreateTexture2D(gfx, probe_width * 2, probe_height, DXGI_FORMAT_R16G16B16A16_FLOAT);
    tex_.probe_SH_coefficients_R.setName("ProbeSHCoefficientsR");
    tex_.probe_SH_coefficients_G = gfxCreateTexture2D(gfx, probe_width * 2, probe_height, DXGI_FORMAT_R16G16B16A16_FLOAT);
    tex_.probe_SH_coefficients_G.setName("ProbeSHCoefficientsG");
    tex_.probe_SH_coefficients_B = gfxCreateTexture2D(gfx, probe_width * 2, probe_height, DXGI_FORMAT_R16G16B16A16_FLOAT);
    tex_.probe_SH_coefficients_B.setName("ProbeSHCoefficientsB");
    tex_.probe_irradiance = gfxCreateTexture2D(gfx, probe_width, probe_height, DXGI_FORMAT_R16G16B16A16_FLOAT);
    tex_.probe_irradiance.setName("ProbeIrradiance");
    tex_.probe_history_trust = gfxCreateTexture2D(gfx, probe_width, probe_height, DXGI_FORMAT_R32_FLOAT);
    tex_.probe_history_trust.setName("ProbeHistoryTrust");
    tex_.tile_adaptive_probe_count[0] = gfxCreateTexture2D(gfx, probe_width, probe_height, DXGI_FORMAT_R32_UINT);
    tex_.tile_adaptive_probe_count[0].setName("TileAdaptiveProbeCount0");
    tex_.tile_adaptive_probe_count[1] = gfxCreateTexture2D(gfx, probe_width, probe_height, DXGI_FORMAT_R32_UINT);
    tex_.tile_adaptive_probe_count[1].setName("TileAdaptiveProbeCount1");
    tex_.tile_adaptive_probe_index[0] = gfxCreateTexture2D(gfx, probe_width * TILE_SIZE, probe_height * TILE_SIZE, DXGI_FORMAT_R16_UINT);
    tex_.tile_adaptive_probe_index[0].setName("TileAdaptiveProbeIndex0");
    tex_.tile_adaptive_probe_index[1] = gfxCreateTexture2D(gfx, probe_width * TILE_SIZE, probe_height * TILE_SIZE, DXGI_FORMAT_R16_UINT);
    tex_.tile_adaptive_probe_index[1].setName("TileAdaptiveProbeIndex1");

    tex_.direct_illumination[0] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.direct_illumination[0].setName("DirectIllumination0");
    tex_.direct_illumination[1] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.direct_illumination[1].setName("DirectIllumination1");
    tex_.filtered_direct_illumination = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.filtered_direct_illumination.setName("FilteredDirectIllumination");

    tex_.indirect_illumination[0] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.indirect_illumination[0].setName("IndirectIllumination0");
    tex_.indirect_illumination[1] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.indirect_illumination[1].setName("IndirectIllumination1");
    tex_.filtered_indirect_illumination = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.filtered_indirect_illumination.setName("FilteredIndirectIllumination");

    tex_.radiance[0] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.radiance[0].setName("Radiance0");
    tex_.radiance[1] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.radiance[1].setName("Radiance1");
    tex_.mapped_rgba = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 1, zero_clear_value);
    tex_.mapped_rgba.setName("MappedRGBA");
    tex_.final_rgba = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 1, zero_clear_value);
    tex_.final_rgba.setName("FinalRGBA");

    tex_.history_diffuse_radiance_without_emission = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.history_diffuse_radiance_without_emission.setName("HistoryRadianceWithoutEmission");

    tex_.reflection[0] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.reflection[0].setName("Reflection0");
    tex_.reflection[1] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.reflection[1].setName("Reflection1");
    tex_.filtered_reflection = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.filtered_reflection.setName("FilteredReflection");
    tex_.reflection_direction = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R32_UINT, 1, zero_clear_value);
    tex_.reflection_direction.setName("ReflectionDirection");
    tex_.reflection_STD_ray_depth = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.reflection_STD_ray_depth.setName("ReflectionSTDRayDepth");
    tex_.filtered_reflection_STD_ray_depth = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.filtered_reflection_STD_ray_depth.setName("FilteredReflectionSTDRayDepth");
    tex_.fallback_reflection[0] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.fallback_reflection[0].setName("FallbackReflection0");
    tex_.fallback_reflection[1] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.fallback_reflection[1].setName("FallbackReflection1");

    tex_.card_workspace_color_alpha = gfxCreateTexture2D(gfx, MAX_CARD_RESOLUTION, MAX_CARD_RESOLUTION, DXGI_FORMAT_R8G8B8A8_UNORM, 1, zero_clear_value);
    tex_.card_workspace_color_alpha.setName("CardWorkspaceColorAlpha");
    tex_.card_workspace_normal = gfxCreateTexture2D(gfx, MAX_CARD_RESOLUTION, MAX_CARD_RESOLUTION, DXGI_FORMAT_R8G8B8A8_UNORM, 1, zero_clear_value);
    tex_.card_workspace_normal.setName("CardWorkspaceNormal");
    tex_.card_workspace_linear_depth = gfxCreateTexture2D(gfx, MAX_CARD_RESOLUTION, MAX_CARD_RESOLUTION, DXGI_FORMAT_R16G16_FLOAT, 1, zero_clear_value);
    tex_.card_workspace_linear_depth.setName("CardWorkspaceLinearDepth");

    tex_.card_atlas_color = gfxCreateTexture2DArray(gfx, CARD_ATLAS_RESOLUTION, CARD_ATLAS_RESOLUTION, NUM_CARD_ATLAS, DXGI_FORMAT_R8G8B8A8_UNORM, 1, zero_clear_value);
    tex_.card_atlas_color.setName("CardAtlasColor");
    tex_.card_atlas_alpha = gfxCreateTexture2DArray(gfx, CARD_ATLAS_RESOLUTION, CARD_ATLAS_RESOLUTION, NUM_CARD_ATLAS, DXGI_FORMAT_R8_UNORM, 1, zero_clear_value);
    tex_.card_atlas_alpha.setName("CardAtlasAlpha");
    tex_.card_atlas_normal = gfxCreateTexture2DArray(gfx, CARD_ATLAS_RESOLUTION, CARD_ATLAS_RESOLUTION, NUM_CARD_ATLAS, DXGI_FORMAT_R8G8B8A8_UNORM, 1, zero_clear_value);
    tex_.card_atlas_normal.setName("CardAtlasNormal");
    tex_.card_atlas_linear_depth = gfxCreateTexture2DArray(gfx, CARD_ATLAS_RESOLUTION, CARD_ATLAS_RESOLUTION, NUM_CARD_ATLAS, DXGI_FORMAT_R16_FLOAT, 1, zero_clear_value);
    tex_.card_atlas_linear_depth.setName("CardAtlasLinearDepth");

    tex_.card_atlas_direct_illumination = gfxCreateTexture2DArray(gfx, CARD_ATLAS_RESOLUTION, CARD_ATLAS_RESOLUTION, NUM_CARD_ATLAS, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.card_atlas_direct_illumination.setName("CardAtlasDirectIllumination");
    tex_.card_atlas_indirect_illumination = gfxCreateTexture2DArray(gfx, CARD_ATLAS_RESOLUTION, CARD_ATLAS_RESOLUTION, NUM_CARD_ATLAS, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.card_atlas_indirect_illumination.setName("CardAtlasIndirectIllumination");
    tex_.card_atlas_lighting = gfxCreateTexture2DArray(gfx, CARD_ATLAS_RESOLUTION, CARD_ATLAS_RESOLUTION, NUM_CARD_ATLAS, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.card_atlas_lighting.setName("CardAtlasLighting");

    tex_.card_workspace_direct_illumination[0] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.card_workspace_direct_illumination[0].setName("CardWorkspaceDirectIllumination0");
    tex_.card_workspace_direct_illumination[1] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.card_workspace_direct_illumination[1].setName("CardWorkspaceDirectIllumination1");
    tex_.card_workspace_indirect_illumination[0] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.card_workspace_indirect_illumination[0].setName("CardWorkspaceIndirectIllumination0");
    tex_.card_workspace_indirect_illumination[1] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.card_workspace_indirect_illumination[1].setName("CardWorkspaceIndirectIllumination1");
    tex_.card_workspace_lighting[0] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.card_workspace_lighting[0].setName("CardWorkspaceLighting0");
    tex_.card_workspace_lighting[1] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.card_workspace_lighting[1].setName("CardWorkspaceLighting1");

    float one_clear_value[4] = {1, 1, 1, 1};
    tex_.rasterization_depth = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_D32_FLOAT, 1, one_clear_value);
    tex_.rasterization_depth.setName("RasterizationDepth");

    tex_.shading_LUT = gfxCreateTexture2D(gfx, 32, 32, DXGI_FORMAT_R16G16_FLOAT);
    tex_.shading_LUT.setName("ShadingLUT");

    return true;
}

void Renderer::DestroyResources() {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    gfxDestroyBuffer(gfx, buf_.dispatch_indirect_command);
    gfxDestroyBuffer(gfx, buf_.dispatch_rays_indirect_command);
    gfxDestroyBuffer(gfx, buf_.draw_indirect_command);
    gfxDestroyBuffer(gfx, buf_.probe_dispatch_command);
    gfxDestroyBuffer(gfx, buf_.probe_per_lane_dispatch_command);
    gfxDestroyBuffer(gfx, buf_.probe_update_ray_reduce_count);

    gfxDestroyBuffer(gfx, buf_.LightGrid_grid_light_list_allocator);
    gfxDestroyBuffer(gfx, buf_.LightGrid_grid_light_count);
    gfxDestroyBuffer(gfx, buf_.LightGrid_grid_light_list_offset);
    gfxDestroyBuffer(gfx, buf_.LightGrid_grid_light_list);
    gfxDestroyBuffer(gfx, buf_.LightGrid_grid_reservoir_weight);

    gfxDestroyBuffer(gfx, buf_.HashGrids_free_tile_count);
    gfxDestroyBuffer(gfx, buf_.HashGrids_free_tile_list);
    gfxDestroyBuffer(gfx, buf_.HashGrids_bucket_hash);
    gfxDestroyBuffer(gfx, buf_.HashGrids_bucket_tile_index);
    gfxDestroyBuffer(gfx, buf_.HashGrids_tile_timestamp);
    gfxDestroyBuffer(gfx, buf_.HashGrids_tile_bucket_hash);
    gfxDestroyBuffer(gfx, buf_.HashGrids_cell_value);
    gfxDestroyBuffer(gfx, buf_.HashGrids_update_cell_value_X);
    gfxDestroyBuffer(gfx, buf_.HashGrids_update_tile_count);
    gfxDestroyBuffer(gfx, buf_.HashGrids_update_tile_list);
    gfxDestroyBuffer(gfx, buf_.HashGrids_active_tile_count_before_allocation);
    gfxDestroyBuffer(gfx, buf_.HashGrids_active_tile_count[0]);
    gfxDestroyBuffer(gfx, buf_.HashGrids_active_tile_count[1]);
    gfxDestroyBuffer(gfx, buf_.HashGrids_active_tile_list[0]);
    gfxDestroyBuffer(gfx, buf_.HashGrids_active_tile_list[1]);

    gfxDestroyBuffer(gfx, buf_.probe_update_ray_counts);
    gfxDestroyBuffer(gfx, buf_.probe_update_ray_offsets);
    gfxDestroyBuffer(gfx, buf_.probe_all_update_ray_count);
    gfxDestroyBuffer(gfx, buf_.probe_update_ray_probe);
    gfxDestroyBuffer(gfx, buf_.probe_update_ray_direction);
    gfxDestroyBuffer(gfx, buf_.probe_update_ray_result);
    gfxDestroyBuffer(gfx, buf_.probe_update_ray_depth);
    gfxDestroyBuffer(gfx, buf_.probe_update_ray_hit_shade_count);
    gfxDestroyBuffer(gfx, buf_.probe_update_ray_hit_shade_list);
    gfxDestroyBuffer(gfx, buf_.probe_update_ray_resolve_hash_cell_index);
    gfxDestroyBuffer(gfx, buf_.adaptive_probe_count);

    gfxDestroyBuffer(gfx, buf_.active_gaussian_count);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_list);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_indirect);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_indirect_src);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_linear_depth_src);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_linear_depth);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_NDC_position);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_quad_NDC_vector0);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_quad_NDC_vector1);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_color);

    gfxDestroyBuffer(gfx, buf_.ray_count);
    gfxDestroyBuffer(gfx, buf_.ray_to_trace_count[0]);
    gfxDestroyBuffer(gfx, buf_.ray_to_trace_count[1]);
    gfxDestroyBuffer(gfx, buf_.ray_to_trace_list[0]);
    gfxDestroyBuffer(gfx, buf_.ray_to_trace_list[1]);
    gfxDestroyBuffer(gfx, buf_.ray_to_trace_direction);
    gfxDestroyBuffer(gfx, buf_.ray_to_trace_origin);
    gfxDestroyBuffer(gfx, buf_.ray_to_trace_UV_position);
    gfxDestroyBuffer(gfx, buf_.ray_to_trace_seed);
    gfxDestroyBuffer(gfx, buf_.ray_to_trace_flags);
    gfxDestroyBuffer(gfx, buf_.ray_to_trace_result);

    gfxDestroyBuffer(gfx, buf_.direct_illumination_ray_occlusion_threshold);
    gfxDestroyBuffer(gfx, buf_.direct_illumination_ray_contribution);
    gfxDestroyBuffer(gfx, buf_.direct_illumination_ray_probe_update_ray_index);

    gfxDestroyBuffer(gfx, buf_.card_sets);
    gfxDestroyBuffer(gfx, buf_.cards);

    gfxDestroyBuffer(gfx, buf_.UB_pool);

#ifndef NDEBUG
    gfxDestroyBuffer(gfx, buf_.Debug_SSRC_probe_index);

    gfxDestroyBuffer(gfx, buf_.Debug_direct_illumination_pixel_ray_index);
    gfxDestroyBuffer(gfx, buf_.Debug_visualize_ray_count);
    gfxDestroyBuffer(gfx, buf_.Debug_visualize_ray_vertex);
    gfxDestroyBuffer(gfx, buf_.Debug_visualize_ray_color);
    gfxDestroyBuffer(gfx, buf_.Debug_visualize_ray_ray_index);
#endif


    gfxDestroyTexture(gfx, tex_.G_depth);
    gfxDestroyTexture(gfx, tex_.G_albedo_alpha);
    gfxDestroyTexture(gfx, tex_.G_material);
    gfxDestroyTexture(gfx, tex_.G_normal[0]);
    gfxDestroyTexture(gfx, tex_.G_normal[1]);
    gfxDestroyTexture(gfx, tex_.G_gaussian_normal);

    gfxDestroyTexture(gfx, tex_.G_filtered_depth);
    gfxDestroyTexture(gfx, tex_.G_zdepth[0]);
    gfxDestroyTexture(gfx, tex_.G_zdepth[1]);
    gfxDestroyTexture(gfx, tex_.near_HZB);

    gfxDestroyTexture(gfx, tex_.debug);

    gfxDestroyTexture(gfx, tex_.probe_screen_coords[0]);
    gfxDestroyTexture(gfx, tex_.probe_screen_coords[1]);
    gfxDestroyTexture(gfx, tex_.probe_linear_depth[0]);
    gfxDestroyTexture(gfx, tex_.probe_linear_depth[1]);
    gfxDestroyTexture(gfx, tex_.probe_world_position[0]);
    gfxDestroyTexture(gfx, tex_.probe_world_position[1]);
    gfxDestroyTexture(gfx, tex_.probe_normal[0]);
    gfxDestroyTexture(gfx, tex_.probe_normal[1]);

    gfxDestroyTexture(gfx, tex_.probe_color[0]);
    gfxDestroyTexture(gfx, tex_.probe_color[1]);
    gfxDestroyTexture(gfx, tex_.probe_sample_color);
    gfxDestroyTexture(gfx, tex_.probe_SH_coefficients_R);
    gfxDestroyTexture(gfx, tex_.probe_SH_coefficients_G);
    gfxDestroyTexture(gfx, tex_.probe_SH_coefficients_B);
    gfxDestroyTexture(gfx, tex_.probe_irradiance);
    gfxDestroyTexture(gfx, tex_.probe_history_trust);
    gfxDestroyTexture(gfx, tex_.tile_adaptive_probe_count[0]);
    gfxDestroyTexture(gfx, tex_.tile_adaptive_probe_count[1]);
    gfxDestroyTexture(gfx, tex_.tile_adaptive_probe_index[0]);
    gfxDestroyTexture(gfx, tex_.tile_adaptive_probe_index[1]);

    gfxDestroyTexture(gfx, tex_.direct_illumination[0]);
    gfxDestroyTexture(gfx, tex_.direct_illumination[1]);
    gfxDestroyTexture(gfx, tex_.filtered_direct_illumination);
    gfxDestroyTexture(gfx, tex_.indirect_illumination[0]);
    gfxDestroyTexture(gfx, tex_.indirect_illumination[1]);
    gfxDestroyTexture(gfx, tex_.filtered_indirect_illumination);
    gfxDestroyTexture(gfx, tex_.radiance[0]);
    gfxDestroyTexture(gfx, tex_.radiance[1]);
    gfxDestroyTexture(gfx, tex_.mapped_rgba);
    gfxDestroyTexture(gfx, tex_.final_rgba);
    gfxDestroyTexture(gfx, tex_.history_diffuse_radiance_without_emission);
    gfxDestroyTexture(gfx, tex_.reflection[0]);
    gfxDestroyTexture(gfx, tex_.reflection[1]);
    gfxDestroyTexture(gfx, tex_.filtered_reflection);
    gfxDestroyTexture(gfx, tex_.reflection_direction);
    gfxDestroyTexture(gfx, tex_.reflection_STD_ray_depth);
    gfxDestroyTexture(gfx, tex_.filtered_reflection_STD_ray_depth);
    gfxDestroyTexture(gfx, tex_.fallback_reflection[0]);
    gfxDestroyTexture(gfx, tex_.fallback_reflection[1]);

    gfxDestroyTexture(gfx, tex_.card_atlas_color);
    gfxDestroyTexture(gfx, tex_.card_atlas_alpha);
    gfxDestroyTexture(gfx, tex_.card_atlas_normal);
    gfxDestroyTexture(gfx, tex_.card_atlas_linear_depth);
    gfxDestroyTexture(gfx, tex_.card_atlas_direct_illumination);
    gfxDestroyTexture(gfx, tex_.card_atlas_indirect_illumination);
    gfxDestroyTexture(gfx, tex_.card_atlas_lighting);
    gfxDestroyTexture(gfx, tex_.card_workspace_direct_illumination[0]);
    gfxDestroyTexture(gfx, tex_.card_workspace_direct_illumination[1]);
    gfxDestroyTexture(gfx, tex_.card_workspace_indirect_illumination[0]);
    gfxDestroyTexture(gfx, tex_.card_workspace_indirect_illumination[1]);
    gfxDestroyTexture(gfx, tex_.card_workspace_lighting[0]);
    gfxDestroyTexture(gfx, tex_.card_workspace_lighting[1]);

    gfxDestroyTexture(gfx, tex_.rasterization_depth);

    gfxDestroyTexture(gfx, tex_.shading_LUT);
}

bool Renderer::CreateKernels () {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    program_ = gfxCreateProgram(gfx, "src/shaders/3dgs",
                                AppInternal::GetInstance().GetRootPath().c_str());
    app_assert(program_);

    std::vector<std::string> defines;
    {
        defines.push_back("WAVE_SIZE=" + std::to_string(cfg_.wave_lane_count));

        if (!options_.no_G_buffers) {
            defines.push_back("OUTPUT_PBR_G_BUFFER");
        }
        if (options_.reconstruct_normals) {
            defines.push_back("RECONSTRUCT_NORMALS_FROM_DEPTH");
        }

        defines.push_back("FILTER_RADIUS=" + std::to_string(options_.filter_radius));
    }

    std::vector<const char *> defines_c;
    for(int i = 0; i < defines.size(); i++) {
        defines_c.push_back(defines[i].c_str());
    }

    // Compute kernels

    {
        kernel_.GenerateRTMesh = gfxCreateComputeKernel(gfx, program_, "GenerateRTMesh", defines_c.data(), defines_c.size());

        kernel_.GenerateDispatchRaysIndirect = gfxCreateComputeKernel(gfx, program_, "GenerateDispatchRaysIndirect",
                                                                     defines_c.data(), defines_c.size());
        kernel_.GenerateDispatchIndirect = gfxCreateComputeKernel(gfx, program_, "GenerateDispatchIndirect",
                                                                  defines_c.data(), defines_c.size());
        kernel_.GenerateDrawIndirect = gfxCreateComputeKernel(gfx, program_, "GenerateDrawIndirect",
                                                              defines_c.data(), defines_c.size());

        kernel_.ClearCounters = gfxCreateComputeKernel(gfx, program_, "ClearCounters", defines_c.data(), defines_c.size());
        kernel_.FilterActiveGaussians = gfxCreateComputeKernel(gfx, program_, "FilterActiveGaussians",
                                                               defines_c.data(), defines_c.size());
        kernel_.ProjectActiveGaussians = gfxCreateComputeKernel(gfx, program_, "ProjectActiveGaussians",
                                                                defines_c.data(), defines_c.size());
        kernel_.ResolveGBuffers = gfxCreateComputeKernel(gfx, program_, "ResolveGBuffers", defines_c.data(), defines_c.size());
        kernel_.FilterDepth     = gfxCreateComputeKernel(gfx, program_, "FilterDepth", defines_c.data(), defines_c.size());
        kernel_.CombineGBuffers = gfxCreateComputeKernel(gfx, program_, "CombineGBuffers", defines_c.data(), defines_c.size());
        kernel_.GenerateNearHZB = gfxCreateComputeKernel(gfx, program_, "GenerateNearHZB", defines_c.data(), defines_c.size());
        kernel_.ReconstructNormals = gfxCreateComputeKernel(gfx, program_, "ReconstructNormals", defines_c.data(), defines_c.size());
        kernel_.InitializeCounters = gfxCreateComputeKernel(gfx, program_, "InitializeCounters", defines_c.data(), defines_c.size());
        kernel_.UpdateLightHeaders = gfxCreateComputeKernel(gfx, program_, "UpdateLightHeaders", defines_c.data(), defines_c.size());
        kernel_.InjectLights = gfxCreateComputeKernel(gfx, program_, "InjectLights", defines_c.data(), defines_c.size());
        kernel_.SampleLightRays = gfxCreateComputeKernel(gfx, program_, "SampleLightRays", defines_c.data(), defines_c.size());
        kernel_.TraceRaysInScreenSpace = gfxCreateComputeKernel(gfx, program_, "TraceRaysInScreenSpace", defines_c.data(), defines_c.size());
        kernel_.CompactRayTraces = gfxCreateComputeKernel(gfx, program_, "CompactRayTraces", defines_c.data(), defines_c.size());
        kernel_.ResolveDirectLighting = gfxCreateComputeKernel(gfx, program_, "ResolveDirectLighting", defines_c.data(), defines_c.size());
        defines_c.push_back("FILTER_PASS=0");
        kernel_.SpatialFilterDirectIllumination[0] = gfxCreateComputeKernel(gfx, program_, "SpatialFilterDirectIllumination", defines_c.data(), defines_c.size());
        defines_c.pop_back();
        defines_c.push_back("FILTER_PASS=1");
        kernel_.SpatialFilterDirectIllumination[1] = gfxCreateComputeKernel(gfx, program_, "SpatialFilterDirectIllumination", defines_c.data(), defines_c.size());
        defines_c.pop_back();
        kernel_.SSRC_ResetHashGrids = gfxCreateComputeKernel(gfx, program_, "SSRC_ResetHashGrids", defines_c.data(), defines_c.size());
        kernel_.SSRC_ReInsertHashGridTiles = gfxCreateComputeKernel(gfx, program_, "SSRC_ReInsertHashGridTiles", defines_c.data(), defines_c.size());
        kernel_.SSRC_AllocateUniformProbes = gfxCreateComputeKernel(gfx, program_, "SSRC_AllocateUniformProbes", defines_c.data(), defines_c.size());
        for (int i = 0; i < SSRC_MAX_ADAPTIVE_PROBE_LAYERS; i++) {
            std::string macro = "SSRC_ADAPTIVE_PROBE_LAYER=" + std::to_string(i);
            defines_c.push_back(macro.c_str());
            kernel_.SSRC_AllocateAdaptiveProbes[i] = gfxCreateComputeKernel(gfx, program_, "SSRC_AllocateAdaptiveProbes", defines_c.data(), defines_c.size());
            defines_c.pop_back();
        }
        kernel_.SSRC_PrepareProbeProcessing = gfxCreateComputeKernel(gfx, program_, "SSRC_PrepareProbeProcessing", defines_c.data(), defines_c.size());
        kernel_.SSRC_ReprojectProbeHistory = gfxCreateComputeKernel(gfx, program_, "SSRC_ReprojectProbeHistory", defines_c.data(), defines_c.size());
        kernel_.SSRC_AllocateProbeUpdateRays = gfxCreateComputeKernel(gfx, program_, "SSRC_AllocateProbeUpdateRays", defines_c.data(), defines_c.size());
        kernel_.SSRC_SetRayCounts = gfxCreateComputeKernel(gfx, program_, "SSRC_SetRayCounts", defines_c.data(), defines_c.size());
        kernel_.SSRC_SampleProbeUpdateRays = gfxCreateComputeKernel(gfx, program_, "SSRC_SampleProbeUpdateRays", defines_c.data(), defines_c.size());
        defines_c.push_back("SSRC_PROBE_UPDATE_RAY_TRACING");
        kernel_.TraceRaysInScreenSpaceForSSRC = gfxCreateComputeKernel(gfx, program_, "TraceRaysInScreenSpace", defines_c.data(), defines_c.size());
        defines_c.pop_back();
        kernel_.SSRC_ResolveRayDepths = gfxCreateComputeKernel(gfx, program_, "SSRC_ResolveRayDepths", defines_c.data(), defines_c.size());
        kernel_.SSRC_ResolveHitLightingFromScreenHistory = gfxCreateComputeKernel(gfx, program_, "SSRC_ResolveHitLightingFromScreenHistory", defines_c.data(), defines_c.size());
        kernel_.SSRC_SampleLightRays = gfxCreateComputeKernel(gfx, program_, "SSRC_SampleLightRays", defines_c.data(), defines_c.size());
        kernel_.SSRC_PrepareClearNewHashGridTileCells = gfxCreateComputeKernel(gfx, program_, "SSRC_PrepareClearNewHashGridTileCells", defines_c.data(), defines_c.size());
        kernel_.SSRC_ClearNewHashGridTileCells = gfxCreateComputeKernel(gfx, program_, "SSRC_ClearNewHashGridTileCells", defines_c.data(), defines_c.size());
        kernel_.SSRC_ResolveHitDirectLightingFromTraceResult = gfxCreateComputeKernel(gfx, program_, "SSRC_ResolveHitDirectLightingFromTraceResult", defines_c.data(), defines_c.size());
        kernel_.SSRC_FilterHashGrids = gfxCreateComputeKernel(gfx, program_, "SSRC_FilterHashGrids", defines_c.data(), defines_c.size());
        kernel_.SSRC_ResolveProbeUpdateRayRadianceFromCells = gfxCreateComputeKernel(gfx, program_, "SSRC_ResolveProbeUpdateRayRadianceFromCells", defines_c.data(), defines_c.size());
        kernel_.SSRC_UpdateProbes = gfxCreateComputeKernel(gfx, program_, "SSRC_UpdateProbes", defines_c.data(), defines_c.size());
        kernel_.SSRC_FilterProbes = gfxCreateComputeKernel(gfx, program_, "SSRC_FilterProbes", defines_c.data(), defines_c.size());
        kernel_.SSRC_PadProbeTextureEdges = gfxCreateComputeKernel(gfx, program_, "SSRC_PadProbeTextureEdges", defines_c.data(), defines_c.size());
        kernel_.SSRC_Integrate = gfxCreateComputeKernel(gfx, program_, "SSRC_Integrate", defines_c.data(), defines_c.size());
        kernel_.FixupIndirectRadianceHoles = gfxCreateComputeKernel(gfx, program_, "FixupIndirectRadianceHoles", defines_c.data(), defines_c.size());
        kernel_.SpawnReflectionRays = gfxCreateComputeKernel(gfx, program_, "SpawnReflectionRays", defines_c.data(), defines_c.size());
        defines_c.push_back("REFLECTION_RAY_TRACING");
        kernel_.TraceRaysInScreenSpaceForReflection = gfxCreateComputeKernel(gfx, program_, "TraceRaysInScreenSpace", defines_c.data(), defines_c.size());
        defines_c.pop_back();
        kernel_.ResolveReflectionTraceResults = gfxCreateComputeKernel(gfx, program_, "ResolveReflectionTraceResults", defines_c.data(), defines_c.size());
        defines_c.push_back("FILTER_PASS=0");
        kernel_.SpatialFilterReflection[0] = gfxCreateComputeKernel(gfx, program_, "SpatialFilterReflection", defines_c.data(), defines_c.size());
        defines_c.pop_back();
        defines_c.push_back("FILTER_PASS=1");
        kernel_.SpatialFilterReflection[1] = gfxCreateComputeKernel(gfx, program_, "SpatialFilterReflection", defines_c.data(), defines_c.size());
        defines_c.pop_back();
        defines_c.push_back("FILTER_PASS=2");
        kernel_.SpatialFilterReflection[2] = gfxCreateComputeKernel(gfx, program_, "SpatialFilterReflection", defines_c.data(), defines_c.size());
        defines_c.pop_back();
        kernel_.TemporalDenoiseReflection = gfxCreateComputeKernel(gfx, program_, "TemporalDenoiseReflection", defines_c.data(), defines_c.size());
        kernel_.TemporalDenoiseLighting = gfxCreateComputeKernel(gfx, program_, "TemporalDenoiseLighting", defines_c.data(), defines_c.size());
        kernel_.FinalComposition = gfxCreateComputeKernel(gfx, program_, "FinalComposition", defines_c.data(), defines_c.size());
        kernel_.ToneMap = gfxCreateComputeKernel(gfx, program_, "ToneMap", defines_c.data(), defines_c.size());

        defines_c.push_back("CARD_SHADERS");
        kernel_.ClearCard = gfxCreateComputeKernel(gfx, program_, "ClearCard", defines_c.data(), defines_c.size());
        kernel_.FilterActiveGaussiansForCard = gfxCreateComputeKernel(gfx, program_, "FilterActiveGaussians", defines_c.data(), defines_c.size());
        kernel_.ProjectActiveGaussiansForCard = gfxCreateComputeKernel(gfx, program_, "ProjectActiveGaussians", defines_c.data(), defines_c.size());
        kernel_.ResolveGBuffersForCard = gfxCreateComputeKernel(gfx, program_, "ResolveGBuffers", defines_c.data(), defines_c.size());
        kernel_.CopyCardToAtlas = gfxCreateComputeKernel(gfx, program_, "CopyCardToAtlas", defines_c.data(), defines_c.size());
        defines_c.pop_back();

        kernel_.SpawnCameraRays = gfxCreateComputeKernel(gfx, program_, "SpawnCameraRays", defines_c.data(),
                                                         defines_c.size());
        kernel_.DisplayCameraRays = gfxCreateComputeKernel(gfx, program_, "DisplayCameraRays", defines_c.data(),
                                                           defines_c.size());
        kernel_.VisualizeMeshCardScene = gfxCreateComputeKernel(gfx, program_, "VisualizeMeshCardScene", defines_c.data(),
                                                                defines_c.size());
        kernel_.VisualizeMeshCardAtlas = gfxCreateComputeKernel(gfx, program_, "VisualizeMeshCardAtlas", defines_c.data(),
                                                                defines_c.size());
        kernel_.SimpleMeshPathTracing = gfxCreateComputeKernel(gfx, program_, "SimpleMeshPathTracing", defines_c.data(),
                                                                      defines_c.size());

        kernel_.Debug_SSRC_VisualizeProbes = gfxCreateComputeKernel(gfx, program_, "Debug_SSRC_VisualizeProbes", defines_c.data(), defines_c.size());
        kernel_.Debug_SSRC_PrepareVisualizeProbeUpdateRays = gfxCreateComputeKernel(gfx, program_, "Debug_SSRC_PrepareVisualizeProbeUpdateRays", defines_c.data(), defines_c.size());
    }

    // Raytracing kernels
    {
        std::vector<char const *> base_subobjects;
        base_subobjects.push_back("PipelineConfig");

        std::vector<char const *> Trace3DGS_kernel_exports;
        Trace3DGS_kernel_exports.push_back("Trace3DGSRaygen");
        Trace3DGS_kernel_exports.push_back("Trace3DGSAnyHit");
        Trace3DGS_kernel_exports.push_back("Trace3DGSMiss");
        std::vector<char const *> Trace3DGS_kernel_subobjects = base_subobjects;
        Trace3DGS_kernel_subobjects.push_back("Trace3DGSHitGroup");
        Trace3DGS_kernel_subobjects.push_back("Trace3DGSShaderConfig");
        kernel_.Trace3DGSRays = gfxCreateRaytracingKernel(gfx, program_, nullptr, 0,
               Trace3DGS_kernel_exports.data(), (uint32_t)Trace3DGS_kernel_exports.size(),
               Trace3DGS_kernel_subobjects.data(), (uint32_t)Trace3DGS_kernel_subobjects.size(),
               defines_c.data(), defines_c.size());

        std::vector<char const *> Trace3DGSShadow_kernel_exports;
        Trace3DGSShadow_kernel_exports.push_back("Trace3DGSShadowRaygen");
        Trace3DGSShadow_kernel_exports.push_back("Trace3DGSStochasticAnyHit");
        Trace3DGSShadow_kernel_exports.push_back("Trace3DGSStochasticMiss");
        std::vector<char const *> Trace3DGSShadow_kernel_subobjects = base_subobjects;
        Trace3DGSShadow_kernel_subobjects.push_back("Trace3DGSShadowHitGroup");
        Trace3DGSShadow_kernel_subobjects.push_back("Trace3DGSShadowShaderConfig");
        kernel_.Trace3DGSShadowRays = gfxCreateRaytracingKernel(gfx, program_, nullptr, 0,
            Trace3DGSShadow_kernel_exports.data(), (uint32_t)Trace3DGSShadow_kernel_exports.size(),
            Trace3DGSShadow_kernel_subobjects.data(), (uint32_t)Trace3DGSShadow_kernel_subobjects.size(),
            defines_c.data(), defines_c.size()
        );

        std::vector<char const *> Trace3DGSShadowRaysWithoutIndirectionList_kernel_exports;
        Trace3DGSShadowRaysWithoutIndirectionList_kernel_exports.push_back("Trace3DGSShadowRaygen");
        Trace3DGSShadowRaysWithoutIndirectionList_kernel_exports.push_back("Trace3DGSStochasticAnyHit");
        Trace3DGSShadowRaysWithoutIndirectionList_kernel_exports.push_back("Trace3DGSStochasticMiss");
        std::vector<char const *> Trace3DGSShadowRaysWithoutIndirectionList_kernel_subobjects = base_subobjects;
        Trace3DGSShadowRaysWithoutIndirectionList_kernel_subobjects.push_back("Trace3DGSShadowHitGroup");
        Trace3DGSShadowRaysWithoutIndirectionList_kernel_subobjects.push_back("Trace3DGSShadowShaderConfig");
        defines_c.push_back("NO_RAY_INDIRECTION_LIST");
        kernel_.Trace3DGSShadowRaysWithoutIndirectionList = gfxCreateRaytracingKernel(gfx, program_, nullptr, 0,
            Trace3DGSShadowRaysWithoutIndirectionList_kernel_exports.data(), (uint32_t)Trace3DGSShadowRaysWithoutIndirectionList_kernel_exports.size(),
            Trace3DGSShadowRaysWithoutIndirectionList_kernel_subobjects.data(), (uint32_t)Trace3DGSShadowRaysWithoutIndirectionList_kernel_subobjects.size(),
            defines_c.data(), defines_c.size()
        );
        defines_c.pop_back();

        std::vector<char const *> DirectIlluminationTrace3DGSShadow_kernel_exports;
        DirectIlluminationTrace3DGSShadow_kernel_exports.push_back("DirectIlluminationTrace3DGSShadowRaygen");
        DirectIlluminationTrace3DGSShadow_kernel_exports.push_back("Trace3DGSStochasticAnyHit");
        DirectIlluminationTrace3DGSShadow_kernel_exports.push_back("Trace3DGSStochasticMiss");
        std::vector<char const *> DirectIlluminationTrace3DGSShadow_kernel_subobjects = base_subobjects;
        DirectIlluminationTrace3DGSShadow_kernel_subobjects.push_back("Trace3DGSShadowHitGroup");
        DirectIlluminationTrace3DGSShadow_kernel_subobjects.push_back("Trace3DGSShadowShaderConfig");
        defines_c.push_back("DIRECT_ILLUMINATION_SHADOW_RAY_TRACING");
        kernel_.DirectIlluminationTrace3DGSShadowRays = gfxCreateRaytracingKernel(gfx, program_, nullptr, 0,
            DirectIlluminationTrace3DGSShadow_kernel_exports.data(), (uint32_t)DirectIlluminationTrace3DGSShadow_kernel_exports.size(),
            DirectIlluminationTrace3DGSShadow_kernel_subobjects.data(), (uint32_t)DirectIlluminationTrace3DGSShadow_kernel_subobjects.size(),
            defines_c.data(), defines_c.size()
        );
        defines_c.pop_back();

        std::vector<char const *> Trace3DGSProbeUpdateRays_kernel_exports;
        Trace3DGSProbeUpdateRays_kernel_exports.push_back("Trace3DGSProbeUpdateRaysRaygen");
        Trace3DGSProbeUpdateRays_kernel_exports.push_back("Trace3DGSStochasticAnyHit");
        Trace3DGSProbeUpdateRays_kernel_exports.push_back("Trace3DGSStochasticClosestHit");
        Trace3DGSProbeUpdateRays_kernel_exports.push_back("Trace3DGSStochasticMiss");
        std::vector<char const *> Trace3DGSProbeUpdateRays_kernel_subobjects = base_subobjects;
        Trace3DGSProbeUpdateRays_kernel_subobjects.push_back("Trace3DGSStochasticHitGroup");
        Trace3DGSProbeUpdateRays_kernel_subobjects.push_back("Trace3DGSStochasticRayShaderConfig");
        defines_c.push_back("PROBE_UPDATE_STOCHASTIC_RAY_TRACING");
        kernel_.Trace3DGSProbeUpdateRays = gfxCreateRaytracingKernel(gfx, program_, nullptr, 0,
            Trace3DGSProbeUpdateRays_kernel_exports.data(), (uint32_t)Trace3DGSProbeUpdateRays_kernel_exports.size(),
            Trace3DGSProbeUpdateRays_kernel_subobjects.data(), (uint32_t)Trace3DGSProbeUpdateRays_kernel_subobjects.size(),
            defines_c.data(), defines_c.size()
        );
        defines_c.pop_back();

        std::vector<char const *> Trace3DGSReflectionRays_kernel_exports;
        Trace3DGSReflectionRays_kernel_exports.push_back("Trace3DGSReflectionRaysRaygen");
        Trace3DGSReflectionRays_kernel_exports.push_back("Trace3DGSStochasticAnyHit");
        Trace3DGSReflectionRays_kernel_exports.push_back("Trace3DGSStochasticClosestHit");
        Trace3DGSReflectionRays_kernel_exports.push_back("Trace3DGSStochasticMiss");
        std::vector<char const *> Trace3DGSReflectionRays_kernel_subobjects = base_subobjects;
        Trace3DGSReflectionRays_kernel_subobjects.push_back("Trace3DGSStochasticHitGroup");
        Trace3DGSReflectionRays_kernel_subobjects.push_back("Trace3DGSStochasticRayShaderConfig");
        defines_c.push_back("REFLECTION_STOCHASTIC_RAY_TRACING");
        kernel_.Trace3DGSReflectionRays = gfxCreateRaytracingKernel(gfx, program_, nullptr, 0,
            Trace3DGSReflectionRays_kernel_exports.data(), (uint32_t)Trace3DGSReflectionRays_kernel_exports.size(),
            Trace3DGSReflectionRays_kernel_subobjects.data(), (uint32_t)Trace3DGSReflectionRays_kernel_subobjects.size(),
            defines_c.data(), defines_c.size()
        );
        defines_c.pop_back();

        std::vector<char const *> Trace3DGSStochasticRays_kernel_exports;
        Trace3DGSStochasticRays_kernel_exports.push_back("Trace3DGSStochasticRaysRaygen");
        Trace3DGSStochasticRays_kernel_exports.push_back("Trace3DGSStochasticAnyHit");
        Trace3DGSStochasticRays_kernel_exports.push_back("Trace3DGSStochasticClosestHit");
        Trace3DGSStochasticRays_kernel_exports.push_back("Trace3DGSStochasticMiss");
        std::vector<char const *> Trace3DGSStochasticRays_kernel_subobjects = base_subobjects;
        Trace3DGSStochasticRays_kernel_subobjects.push_back("Trace3DGSStochasticHitGroup");
        Trace3DGSStochasticRays_kernel_subobjects.push_back("Trace3DGSStochasticRayShaderConfig");
        defines_c.push_back("STOCHASTIC_RAY_TRACING");
        kernel_.Trace3DGSStochasticRays = gfxCreateRaytracingKernel(gfx, program_, nullptr, 0,
            Trace3DGSStochasticRays_kernel_exports.data(), (uint32_t)Trace3DGSStochasticRays_kernel_exports.size(),
            Trace3DGSStochasticRays_kernel_subobjects.data(), (uint32_t)Trace3DGSStochasticRays_kernel_subobjects.size(),
            defines_c.data(), defines_c.size()
        );
        defines_c.pop_back();

        uint32_t entry_count[kGfxShaderGroupType_Count] {
                1, // 1 raygen record
                1, // 1 hitgroups
                1, // 1 miss
                1 // Actually we have no callables... but leave 1 here.
        };
        GfxKernel sbt_kernels[] {
            kernel_.Trace3DGSRays,
            kernel_.Trace3DGSShadowRays,
            kernel_.Trace3DGSShadowRaysWithoutIndirectionList,
            kernel_.DirectIlluminationTrace3DGSShadowRays,
            kernel_.Trace3DGSProbeUpdateRays,
            kernel_.Trace3DGSReflectionRays,
            kernel_.Trace3DGSStochasticRays
        };
        sbt_ = gfxCreateSbt(gfx, sbt_kernels, ARRAYSIZE(sbt_kernels), entry_count);
    }

    // Graphics kernels

    {
        GfxDrawState draw_state = {};
        // Cull gaussian fragments blocked by regular meshes
        gfxDrawStateSetDepthStencilTarget(draw_state, DXGI_FORMAT_D32_FLOAT);
        gfxDrawStateSetDepthFunction(draw_state, D3D12_COMPARISON_FUNC_LESS);
        gfxDrawStateSetDepthWriteMask(draw_state, D3D12_DEPTH_WRITE_MASK_ZERO);
        // This function will reset the blend mode...D
        // gfxDrawStateEnableAlphaBlending(draw_state);
        gfxDrawStateSetBlendMode(draw_state,
                                         D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
                                         D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD);
        gfxDrawStateSetColorTarget(draw_state, 0, tex_.G_albedo_alpha.getFormat());
        if (!options_.no_G_buffers) {
            gfxDrawStateSetColorTarget(draw_state, 1, tex_.G_material.getFormat());
            gfxDrawStateSetColorTarget(draw_state, 2, tex_.G_depth.getFormat());
            if (!options_.reconstruct_normals) {
                gfxDrawStateSetColorTarget(draw_state, 3, tex_.G_gaussian_normal.getFormat());
            }
        }
        gfxDrawStateSetPrimitiveTopologyType(draw_state, D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT);
        gfxDrawStateSetCullMode(draw_state, D3D12_CULL_MODE_NONE);
        kernel_.DrawActiveGaussians = gfxCreateGraphicsKernel(
                gfx, program_, draw_state, "DrawActiveGaussians", defines_c.data(), defines_c.size()
        );
    }
    {
        GfxDrawState draw_state = {};
        gfxDrawStateSetBlendMode(draw_state,
                                         D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
                                         D3D12_BLEND_ONE, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD);
        gfxDrawStateSetColorTarget(draw_state, 0, tex_.card_workspace_color_alpha.getFormat());
        gfxDrawStateSetColorTarget(draw_state, 1, tex_.card_workspace_linear_depth.getFormat());
        gfxDrawStateSetColorTarget(draw_state, 2, tex_.card_workspace_normal.getFormat());

        gfxDrawStateSetPrimitiveTopologyType(draw_state, D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT);
        gfxDrawStateSetCullMode(draw_state, D3D12_CULL_MODE_NONE);
        defines_c.push_back("CARD_SHADERS");
        kernel_.DrawActiveGaussiansForCard = gfxCreateGraphicsKernel(
                gfx, program_, draw_state, "DrawActiveGaussians", defines_c.data(), defines_c.size()
        );
        defines_c.pop_back();
    }
    {
        GfxDrawState draw_state = {};
        gfxDrawStateDisableGeometryShader(draw_state);
        // TODO should be culling backfaces...
        gfxDrawStateSetCullMode(draw_state, D3D12_CULL_MODE_NONE);
        gfxDrawStateSetDepthStencilTarget(draw_state, DXGI_FORMAT_D32_FLOAT);
        gfxDrawStateSetDepthFunction(draw_state, D3D12_COMPARISON_FUNC_LESS);
        gfxDrawStateSetDepthWriteMask(draw_state, D3D12_DEPTH_WRITE_MASK_ALL);
        gfxDrawStateSetColorTarget(draw_state, 0, tex_.G_albedo_alpha.getFormat());
        gfxDrawStateSetColorTarget(draw_state, 1, tex_.G_emission_alpha.getFormat());
        gfxDrawStateSetColorTarget(draw_state, 2, tex_.G_material.getFormat());
        gfxDrawStateSetColorTarget(draw_state, 3, tex_.G_normal[0].getFormat());
        kernel_.DrawRegularMeshes = gfxCreateGraphicsKernel(
                gfx, program_, draw_state, "DrawRegularMeshes", defines_c.data(), defines_c.size()
        );
    }
    {
        GfxDrawState draw_state = {};
        gfxDrawStateDisableGeometryShader(draw_state);
        gfxDrawStateSetColorTarget(draw_state, 0, tex_.final_rgba.getFormat());
        kernel_.AntiAliasing = gfxCreateGraphicsKernel(
                gfx, program_, draw_state, "AntiAliasing", defines_c.data(), defines_c.size());
    }
    {
        GfxDrawState draw_state = {};
        gfxDrawStateDisableGeometryShader(draw_state);
        kernel_.DrawToBackBuffer = gfxCreateGraphicsKernel(
                gfx, program_, draw_state, "DrawToBackBuffer", defines_c.data(), defines_c.size());
    }

    {
        GfxDrawState draw_state = {};
        gfxDrawStateDisableGeometryShader(draw_state);
        gfxDrawStateSetDepthStencilTarget(draw_state, DXGI_FORMAT_D32_FLOAT);
        gfxDrawStateSetDepthFunction(draw_state, D3D12_COMPARISON_FUNC_LESS);
        gfxDrawStateSetDepthWriteMask(draw_state, D3D12_DEPTH_WRITE_MASK_ZERO);
        gfxDrawStateSetColorTarget(draw_state, 0, tex_.debug.getFormat());
        gfxDrawStateSetPrimitiveTopologyType(draw_state, D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE);
        kernel_.Debug_SSRC_VisualizeProbeUpdateRays = gfxCreateGraphicsKernel(
                gfx, program_, draw_state, "Debug_SSRC_VisualizeProbeUpdateRays", defines_c.data(), defines_c.size());
    }

    return true;
}

void Renderer::DestroyKernels () {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    gfxDestroyKernel(gfx, kernel_.GenerateRTMesh);

    gfxDestroyKernel(gfx, kernel_.GenerateDispatchRaysIndirect);
    gfxDestroyKernel(gfx, kernel_.GenerateDispatchIndirect);
    gfxDestroyKernel(gfx, kernel_.GenerateDrawIndirect);

    gfxDestroyKernel(gfx, kernel_.ClearCounters);
    gfxDestroyKernel(gfx, kernel_.FilterActiveGaussians);
    gfxDestroyKernel(gfx, kernel_.ProjectActiveGaussians);
    gfxDestroyKernel(gfx, kernel_.ResolveGBuffers);
    gfxDestroyKernel(gfx, kernel_.FilterDepth);
    gfxDestroyKernel(gfx, kernel_.CombineGBuffers);
    gfxDestroyKernel(gfx, kernel_.GenerateNearHZB);
    gfxDestroyKernel(gfx, kernel_.ReconstructNormals);
    gfxDestroyKernel(gfx, kernel_.InitializeCounters);
    gfxDestroyKernel(gfx, kernel_.UpdateLightHeaders);
    gfxDestroyKernel(gfx, kernel_.InjectLights);
    gfxDestroyKernel(gfx, kernel_.SampleLightRays);
    gfxDestroyKernel(gfx, kernel_.TraceRaysInScreenSpace);
    gfxDestroyKernel(gfx, kernel_.CompactRayTraces);
    gfxDestroyKernel(gfx, kernel_.ResolveDirectLighting);
    gfxDestroyKernel(gfx, kernel_.SpatialFilterDirectIllumination[0]);
    gfxDestroyKernel(gfx, kernel_.SpatialFilterDirectIllumination[1]);
    gfxDestroyKernel(gfx, kernel_.SSRC_ResetHashGrids);
    gfxDestroyKernel(gfx, kernel_.SSRC_ReInsertHashGridTiles);
    gfxDestroyKernel(gfx, kernel_.SSRC_AllocateUniformProbes);
    for (int i = 0; i < SSRC_MAX_ADAPTIVE_PROBE_LAYERS; i ++) {
        gfxDestroyKernel(gfx, kernel_.SSRC_AllocateAdaptiveProbes[i]);
    }
    gfxDestroyKernel(gfx, kernel_.SSRC_PrepareProbeProcessing);
    gfxDestroyKernel(gfx, kernel_.SSRC_ReprojectProbeHistory);
    gfxDestroyKernel(gfx, kernel_.SSRC_AllocateProbeUpdateRays);
    gfxDestroyKernel(gfx, kernel_.SSRC_SetRayCounts);
    gfxDestroyKernel(gfx, kernel_.SSRC_SampleProbeUpdateRays);
    gfxDestroyKernel(gfx, kernel_.TraceRaysInScreenSpaceForSSRC);
    gfxDestroyKernel(gfx, kernel_.SSRC_ResolveRayDepths);
    gfxDestroyKernel(gfx, kernel_.SSRC_ResolveHitLightingFromScreenHistory);
    gfxDestroyKernel(gfx, kernel_.SSRC_SampleLightRays);
    gfxDestroyKernel(gfx, kernel_.SSRC_PrepareClearNewHashGridTileCells);
    gfxDestroyKernel(gfx, kernel_.SSRC_ClearNewHashGridTileCells);
    gfxDestroyKernel(gfx, kernel_.SSRC_ResolveHitDirectLightingFromTraceResult);
    gfxDestroyKernel(gfx, kernel_.SSRC_FilterHashGrids);
    gfxDestroyKernel(gfx, kernel_.SSRC_ResolveProbeUpdateRayRadianceFromCells);
    gfxDestroyKernel(gfx, kernel_.SSRC_UpdateProbes);
    gfxDestroyKernel(gfx, kernel_.SSRC_FilterProbes);
    gfxDestroyKernel(gfx, kernel_.SSRC_PadProbeTextureEdges);
    gfxDestroyKernel(gfx, kernel_.SSRC_Integrate);
    gfxDestroyKernel(gfx, kernel_.FixupIndirectRadianceHoles);
    gfxDestroyKernel(gfx, kernel_.SpawnReflectionRays);
    gfxDestroyKernel(gfx, kernel_.TraceRaysInScreenSpaceForReflection);
    gfxDestroyKernel(gfx, kernel_.ResolveReflectionTraceResults);
    gfxDestroyKernel(gfx, kernel_.SpatialFilterReflection[0]);
    gfxDestroyKernel(gfx, kernel_.SpatialFilterReflection[1]);
    gfxDestroyKernel(gfx, kernel_.SpatialFilterReflection[2]);
    gfxDestroyKernel(gfx, kernel_.TemporalDenoiseReflection);
    gfxDestroyKernel(gfx, kernel_.TemporalDenoiseLighting);
    gfxDestroyKernel(gfx, kernel_.FinalComposition);
    gfxDestroyKernel(gfx, kernel_.ToneMap);

    gfxDestroyKernel(gfx, kernel_.ClearCard);
    gfxDestroyKernel(gfx, kernel_.FilterActiveGaussiansForCard);
    gfxDestroyKernel(gfx, kernel_.ProjectActiveGaussiansForCard);
    gfxDestroyKernel(gfx, kernel_.DrawActiveGaussiansForCard);
    gfxDestroyKernel(gfx, kernel_.ResolveGBuffersForCard);
    gfxDestroyKernel(gfx, kernel_.CopyCardToAtlas);

    gfxDestroyKernel(gfx, kernel_.Trace3DGSRays);
    gfxDestroyKernel(gfx, kernel_.Trace3DGSShadowRays);
    gfxDestroyKernel(gfx, kernel_.Trace3DGSShadowRaysWithoutIndirectionList);
    gfxDestroyKernel(gfx, kernel_.DirectIlluminationTrace3DGSShadowRays);
    gfxDestroyKernel(gfx, kernel_.Trace3DGSProbeUpdateRays);
    gfxDestroyKernel(gfx, kernel_.Trace3DGSReflectionRays);
    gfxDestroyKernel(gfx, kernel_.Trace3DGSStochasticRays);

    gfxDestroyKernel(gfx, kernel_.SpawnCameraRays);
    gfxDestroyKernel(gfx, kernel_.DisplayCameraRays);
    gfxDestroyKernel(gfx, kernel_.VisualizeMeshCardScene);
    gfxDestroyKernel(gfx, kernel_.VisualizeMeshCardAtlas);
    gfxDestroyKernel(gfx, kernel_.SimpleMeshPathTracing);

    gfxDestroyKernel(gfx, kernel_.DrawRegularMeshes);
    gfxDestroyKernel(gfx, kernel_.DrawActiveGaussians);
    gfxDestroyKernel(gfx, kernel_.AntiAliasing);
    gfxDestroyKernel(gfx, kernel_.DrawToBackBuffer);

    gfxDestroyKernel(gfx, kernel_.Debug_SSRC_VisualizeProbes);
    gfxDestroyKernel(gfx, kernel_.Debug_SSRC_PrepareVisualizeProbeUpdateRays);
    gfxDestroyKernel(gfx, kernel_.Debug_SSRC_VisualizeProbeUpdateRays);

    gfxDestroySbt(gfx, sbt_);

    gfxDestroyProgram(gfx, program_);
}

bool Renderer::InitializeConfig () {
    auto gfx = AppInternal::GetInstance().GetGfx();
    auto dx_device = gfxGetDevice(gfx);
    D3D12_FEATURE_DATA_D3D12_OPTIONS1 features = {};
    if (FAILED(dx_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &features, sizeof(features))))
    {
        app_warning("Failed to check feature support");
        return false;
    }
    if (!features.WaveOps)
    {
        app_warning("Wave operations are not supported");
        return false;
    }
    if (features.WaveLaneCountMin != 32)
    {
        app_warning("only 32 wave lanes are supported");
        return false;
    }
    cfg_.wave_lane_count = 32;
    return true;
}

bool Renderer::Initialize () {
    // Reset flags and counters
    should_build_acceleration_structure_ = true;
    should_reset_hash_grids_ = true;
    frame_index_ = 0;

    CB.scene_area_light_count = AppInternal::GetInstance().GetScene().GetNumLights() - 2;

    rng_ = std::mt19937(0);

    if (!InitializeConfig()) return false;

    if (!blue_noise_sampler_.Initialize()) return false;

    if(!CreateResources()) {
        return false;
    }
    if(!CreateKernels()) {
        return false;
    }

    ResetUniformBufferPool();
    ResetStagingBuffers();

    ComputeShadingLUT();

    return true;
}

void Renderer::Destroy() {
    DestroyResources();
    DestroyKernels();
    ResetStagingBuffers();
    blue_noise_sampler_.Destroy();
}

/*
 * Created: 2024/11/14
 * Author:  hineven
 * See LICENSE for licensing.
 */
#include "renderer.h"
#include "3dgs_shared.hlsl"


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


    buf_.LightGrid_grid_light_list_allocator = gfxCreateBuffer<uint>(gfx, 1);
    buf_.LightGrid_grid_light_list_allocator.setName("LightGridGridLightListAllocator");
    int num_light_grids = options_.light_grid_size * options_.light_grid_size * options_.light_grid_size * options_.light_grid_num_cascades;
    buf_.LightGrid_grid_light_count = gfxCreateBuffer<uint>(gfx, num_light_grids);
    buf_.LightGrid_grid_light_count.setName("LightGridGridLightCount");
    buf_.LightGrid_grid_light_list_offset = gfxCreateBuffer<uint>(gfx, num_light_grids);
    buf_.LightGrid_grid_light_list_offset.setName("LightGridGridLightListOffset");
    buf_.LightGrid_grid_light_list = gfxCreateBuffer<uint>(gfx, options_.light_grid_max_num_entries);
    buf_.LightGrid_grid_light_list.setName("LightGridGridLightList");

    buf_.active_gaussian_count = gfxCreateBuffer<int>(gfx, 1);
    buf_.active_gaussian_count.setName("GaussianActiveCount");
    buf_.active_gaussian_list_src = gfxCreateBuffer<int>(gfx, max_num_gaussians);
    buf_.active_gaussian_list_src.setName("ActiveGaussianSrcList");
    buf_.active_gaussian_list = gfxCreateBuffer<int>(gfx, max_num_gaussians);
    buf_.active_gaussian_list.setName("ActiveGaussianList");
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
    buf_.direct_illumination_ray_occlusion_threshold = gfxCreateBuffer<float>(gfx, max_num_pixels);
    buf_.direct_illumination_ray_occlusion_threshold.setName("DirectIlluminationRayOcclusionThreshold");
    buf_.direct_illumination_ray_contribution = gfxCreateBuffer<uint2>(gfx, max_num_pixels);
    buf_.direct_illumination_ray_contribution.setName("DirectIlluminationRayContribution");

#ifndef _NDEBUG
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
    buf_.UB = gfxCreateBuffer(gfx, UB_stride * gfxGetBackBufferCount(gfx), nullptr, kGfxCpuAccess_Write);
    buf_.UB.setName("UniformBlock0");
    buf_.UB.setStride(UB_stride);

    float zero_clear_value[4] = {0, 0, 0, 0};
    tex_.G_depth = gfxCreateTexture2D(gfx, width, height, options_.depth_format, 1, zero_clear_value);
    tex_.G_depth.setName("G_depth");
    tex_.G_albedo_alpha = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 1, zero_clear_value);
    tex_.G_albedo_alpha.setName("G_albedo_alpha");
    tex_.G_material = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8_UNORM, 1, zero_clear_value);
    tex_.G_material.setName("G_material");
    tex_.G_normal = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 1, zero_clear_value);
    tex_.G_normal.setName("G_normal");
    tex_.G_zdepth[0] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R32_FLOAT, 1, zero_clear_value);
    tex_.G_zdepth[0].setName("G_zdepth0");
    tex_.G_zdepth[1] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R32_FLOAT, 1, zero_clear_value);
    tex_.G_zdepth[1].setName("G_zdepth1");
    int num_mips = gfxCalculateMipCount(width, height);
    assert(num_mips > 1);
    tex_.near_HZB = gfxCreateTexture2D(gfx, divideAndRoundUp(width, 2), divideAndRoundUp(height, 2), DXGI_FORMAT_R32_FLOAT, num_mips - 1, zero_clear_value);
    tex_.near_HZB.setName("NearHZB");

    tex_.debug = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.debug.setName("Debug");

    tex_.G_filtered_depth = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R32_FLOAT, 1, zero_clear_value);
    tex_.G_filtered_depth.setName("G_filtered_depth");

    tex_.direct_illumination[0] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.direct_illumination[0].setName("DirectIllumination0");
    tex_.direct_illumination[1] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.direct_illumination[1].setName("DirectIllumination1");
    tex_.filtered_direct_illumination = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.filtered_direct_illumination.setName("FilteredDirectIllumination");

    tex_.radiance[0] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.radiance[0].setName("Radiance0");
    tex_.radiance[1] = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, 1, zero_clear_value);
    tex_.radiance[1].setName("Radiance1");

    return true;
}

void Renderer::DestroyResources() {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    gfxDestroyBuffer(gfx, buf_.dispatch_indirect_command);
    gfxDestroyBuffer(gfx, buf_.dispatch_rays_indirect_command);
    gfxDestroyBuffer(gfx, buf_.draw_indirect_command);

    gfxDestroyBuffer(gfx, buf_.LightGrid_grid_light_list_allocator);
    gfxDestroyBuffer(gfx, buf_.LightGrid_grid_light_count);
    gfxDestroyBuffer(gfx, buf_.LightGrid_grid_light_list_offset);
    gfxDestroyBuffer(gfx, buf_.LightGrid_grid_light_list);

    gfxDestroyBuffer(gfx, buf_.active_gaussian_count);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_list_src);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_list);
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

    gfxDestroyBuffer(gfx, buf_.UB);

#ifndef NDEBUG
    gfxDestroyBuffer(gfx, buf_.Debug_direct_illumination_pixel_ray_index);
    gfxDestroyBuffer(gfx, buf_.Debug_visualize_ray_count);
    gfxDestroyBuffer(gfx, buf_.Debug_visualize_ray_vertex);
    gfxDestroyBuffer(gfx, buf_.Debug_visualize_ray_color);
    gfxDestroyBuffer(gfx, buf_.Debug_visualize_ray_ray_index);
#endif


    gfxDestroyTexture(gfx, tex_.G_depth);
    gfxDestroyTexture(gfx, tex_.G_albedo_alpha);
    gfxDestroyTexture(gfx, tex_.G_material);
    gfxDestroyTexture(gfx, tex_.G_normal);

    gfxDestroyTexture(gfx, tex_.G_filtered_depth);
    gfxDestroyTexture(gfx, tex_.G_zdepth[0]);
    gfxDestroyTexture(gfx, tex_.G_zdepth[1]);
    gfxDestroyTexture(gfx, tex_.near_HZB);

    gfxDestroyTexture(gfx, tex_.debug);

    gfxDestroyTexture(gfx, tex_.direct_illumination[0]);
    gfxDestroyTexture(gfx, tex_.direct_illumination[1]);
    gfxDestroyTexture(gfx, tex_.filtered_direct_illumination);
    gfxDestroyTexture(gfx, tex_.radiance[0]);
    gfxDestroyTexture(gfx, tex_.radiance[1]);
}

bool Renderer::CreateKernels () {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    program_ = gfxCreateProgram(gfx, "src/shaders/3dgs",
                                AppInternal::GetInstance().GetRootPath().c_str());
    app_assert(program_);

    std::vector<std::string> defines;
    {
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
        defines.push_back("WAVE_SIZE=32");

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
        kernel_.FilterDepth  = gfxCreateComputeKernel(gfx, program_, "FilterDepth", defines_c.data(), defines_c.size());
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
        kernel_.TemporalFilterDirectIllumination = gfxCreateComputeKernel(gfx, program_, "TemporalFilterDirectIllumination", defines_c.data(), defines_c.size());

        kernel_.FinalComposition = gfxCreateComputeKernel(gfx, program_, "FinalComposition", defines_c.data(), defines_c.size());

        kernel_.SpawnCameraRays = gfxCreateComputeKernel(gfx, program_, "SpawnCameraRays", defines_c.data(),
                                                         defines_c.size());
        kernel_.DisplayCameraRays = gfxCreateComputeKernel(gfx, program_, "DisplayCameraRays", defines_c.data(),
                                                           defines_c.size());
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
        Trace3DGSShadow_kernel_exports.push_back("Trace3DGSShadowAnyHit");
        Trace3DGSShadow_kernel_exports.push_back("Trace3DGSShadowMiss");
        std::vector<char const *> Trace3DGSShadow_kernel_subobjects = base_subobjects;
        Trace3DGSShadow_kernel_subobjects.push_back("Trace3DGSShadowHitGroup");
        Trace3DGSShadow_kernel_subobjects.push_back("Trace3DGSShadowShaderConfig");
        kernel_.Trace3DGSShadowRays = gfxCreateRaytracingKernel(gfx, program_, nullptr, 0,
            Trace3DGSShadow_kernel_exports.data(), (uint32_t)Trace3DGSShadow_kernel_exports.size(),
            Trace3DGSShadow_kernel_subobjects.data(), (uint32_t)Trace3DGSShadow_kernel_subobjects.size(),
            defines_c.data(), defines_c.size()
        );


        std::vector<char const *> DirectIlluminationTrace3DGSShadow_kernel_exports;
        DirectIlluminationTrace3DGSShadow_kernel_exports.push_back("DirectIlluminationTrace3DGSShadowRaygen");
        DirectIlluminationTrace3DGSShadow_kernel_exports.push_back("Trace3DGSShadowAnyHit");
        DirectIlluminationTrace3DGSShadow_kernel_exports.push_back("Trace3DGSShadowMiss");
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

        uint32_t entry_count[kGfxShaderGroupType_Count] {
                1, // 1 raygen record
                1, // 1 hitgroups
                1, // 1 miss
                1 // Actually we have no callables... but leave 1 here.
        };
        GfxKernel sbt_kernels[] {kernel_.Trace3DGSRays, kernel_.Trace3DGSShadowRays, kernel_.DirectIlluminationTrace3DGSShadowRays};
        sbt_ = gfxCreateSbt(gfx, sbt_kernels, ARRAYSIZE(sbt_kernels), entry_count);
    }

    // Graphics kernels

    {
        GfxDrawState draw_state = {};
        gfxDrawStateSetDepthStencilTarget(draw_state, DXGI_FORMAT_UNKNOWN);
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
                gfxDrawStateSetColorTarget(draw_state, 3, tex_.G_normal.getFormat());
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
        gfxDrawStateDisableGeometryShader(draw_state);
        kernel_.TonemapAndDraw = gfxCreateGraphicsKernel(
                gfx, program_, draw_state, "TonemapAndDraw", defines_c.data(), defines_c.size());
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
    gfxDestroyKernel(gfx, kernel_.TemporalFilterDirectIllumination);
    gfxDestroyKernel(gfx, kernel_.FinalComposition);

    gfxDestroyKernel(gfx, kernel_.Trace3DGSRays);
    gfxDestroyKernel(gfx, kernel_.Trace3DGSShadowRays);
    gfxDestroyKernel(gfx, kernel_.SpawnCameraRays);
    gfxDestroyKernel(gfx, kernel_.DisplayCameraRays);

    gfxDestroyKernel(gfx, kernel_.DrawActiveGaussians);
    gfxDestroyKernel(gfx, kernel_.TonemapAndDraw);

    gfxDestroySbt(gfx, sbt_);

    gfxDestroyProgram(gfx, program_);
}


bool Renderer::Initialize () {
    // Reset flags and counters
    should_build_acceleration_structure_ = true;
    frame_index_ = 0;

    rng_ = std::mt19937(0);

    if (!blue_noise_sampler_.Initialize()) return false;

    if(!CreateResources()) {
        return false;
    }
    if(!CreateKernels()) {
        return false;
    }


    return true;
}

void Renderer::Destroy() {
    DestroyResources();
    DestroyKernels();
    blue_noise_sampler_.Destroy();
}
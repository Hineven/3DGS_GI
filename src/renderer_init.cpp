/*
 * Created: 2024/11/14
 * Author:  hineven
 * See LICENSE for licensing.
 */
#include "renderer.h"
#include "shaders/3dgs_shared.hlsl"


bool Renderer::CreateResources () {
    // a maximum of 12 million gaussians
    int max_num_gaussians = 1024 * 1024 * 12;
    int max_num_gaussian_instances = 1024 * 1024 * 8 * 8;
    int width = AppInternal::GetInstance().GetWindowWidth();
    int height = AppInternal::GetInstance().GetWindowHeight();
    auto & gfx = AppInternal::GetInstance().GetGfx();
    buf_.dispatch_indirect_command = gfxCreateBuffer<DispatchIndirectCommand>(gfx, 1);
    buf_.dispatch_indirect_command.setName("DispatchIndirectCommand");
    buf_.gaussian_active_count = gfxCreateBuffer<int>(gfx, 1);
    buf_.gaussian_active_count.setName("GaussianActiveCount");
    buf_.active_gaussian_list = gfxCreateBuffer<int>(gfx, max_num_gaussians);
    buf_.active_gaussian_list.setName("ActiveGaussianList");
    buf_.active_gaussian_depth = gfxCreateBuffer<float>(gfx, max_num_gaussians);
    buf_.active_gaussian_depth.setName("ActiveGaussianDepth");
    buf_.active_gaussian_screen_position = gfxCreateBuffer<glm::vec2>(gfx, max_num_gaussians);
    buf_.active_gaussian_screen_position.setName("ActiveGaussianScreenPosition");
    buf_.active_gaussian_screen_radius = gfxCreateBuffer<float>(gfx, max_num_gaussians);
    buf_.active_gaussian_screen_radius.setName("ActiveGaussianScreenRadius");
    buf_.active_gaussian_conic_w = gfxCreateBuffer<glm::vec4>(gfx, max_num_gaussians);
    buf_.active_gaussian_conic_w.setName("ActiveGaussianConicW");
    buf_.active_gaussian_tile_count = gfxCreateBuffer<int>(gfx, max_num_gaussians);
    buf_.active_gaussian_tile_count.setName("ActiveGaussianTileCount");
    buf_.active_gaussian_instance_base = gfxCreateBuffer<int>(gfx, max_num_gaussians);
    buf_.active_gaussian_instance_base.setName("ActiveGaussianInstanceBase");
    buf_.active_gaussian_instance_count = gfxCreateBuffer<int>(gfx, 1);
    buf_.active_gaussian_instance_count.setName("ActiveGaussianInstanceCount");
    buf_.active_gaussian_color = gfxCreateBuffer<glm::vec3>(gfx, max_num_gaussians);
    buf_.active_gaussian_color.setName("ActiveGaussianColor");
    buf_.active_gaussian_instance_key = gfxCreateBuffer<int>(gfx, max_num_gaussian_instances);
    buf_.active_gaussian_instance_key.setName("ActiveGaussianInstanceKey");
    buf_.active_gaussian_instance_key_sorted = gfxCreateBuffer<int>(gfx, max_num_gaussian_instances);
    buf_.active_gaussian_instance_key_sorted.setName("ActiveGaussianInstanceKeySorted");
    buf_.active_gaussian_instance_gaussian_index = gfxCreateBuffer<int>(gfx, max_num_gaussian_instances);
    buf_.active_gaussian_instance_gaussian_index.setName("ActiveGaussianInstanceGaussianIndex");
    buf_.active_gaussian_instance_gaussian_index_sorted = gfxCreateBuffer<int>(gfx, max_num_gaussian_instances);
    buf_.active_gaussian_instance_gaussian_index_sorted.setName("ActiveGaussianInstanceGaussianIndexSorted");
    int num_tiles = divideAndRoundUp(width, TILE_SIZE) * divideAndRoundUp(height, TILE_SIZE);
    buf_.tile_gaussian_instance_start = gfxCreateBuffer<int>(gfx, num_tiles);
    buf_.tile_gaussian_instance_start.setName("TileGaussianInstanceStart");
    buf_.tile_gaussian_instance_end = gfxCreateBuffer<int>(gfx, num_tiles);
    buf_.tile_gaussian_instance_end.setName("TileGaussianInstanceEnd");

    int max_num_rays = options_.max_num_rays;

    buf_.ray_to_trace_count = gfxCreateBuffer<int>(gfx, 1);
    buf_.ray_to_trace_count.setName("RayToTraceCount");
    buf_.ray_to_trace_direction = gfxCreateBuffer<uint>(gfx, max_num_rays);
    buf_.ray_to_trace_direction.setName("RayToTraceDirection");
    buf_.ray_to_trace_origin = gfxCreateBuffer<glm::vec3>(gfx, max_num_rays);
    buf_.ray_to_trace_origin.setName("RayToTraceOrigin");
    buf_.ray_to_trace_t_max = gfxCreateBuffer<float>(gfx, max_num_rays);
    buf_.ray_to_trace_t_max.setName("RayToTraceTMax");
    buf_.ray_to_trace_flags = gfxCreateBuffer<uint>(gfx, max_num_rays);
    buf_.ray_to_trace_flags.setName("RayToTraceFlags");
    buf_.ray_to_trace_result = gfxCreateBuffer<uint2>(gfx, max_num_rays);
    buf_.ray_to_trace_result.setName("RayToTraceResult");

    buf_.UB = gfxCreateBuffer<UniformBlock> (gfx, 2, nullptr, kGfxCpuAccess_Write);
    buf_.UB.setName("UniformBlock");
    buf_.UB.setStride(sizeof(UniformBlock));

    tex_.G_color = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R16G16B16A16_FLOAT);
    tex_.G_color.setName("G_color");

//    tex_.output = gfxCreateTexture2D(gfx, width, height, DXGI_FORMAT_R8G8B8A8_UNORM);

    return true;
}

void Renderer::DestroyResources() {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    gfxDestroyBuffer(gfx, buf_.dispatch_indirect_command);
    gfxDestroyBuffer(gfx, buf_.gaussian_active_count);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_list);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_depth);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_screen_position);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_screen_radius);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_conic_w);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_tile_count);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_instance_base);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_instance_count);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_color);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_instance_key);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_instance_key_sorted);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_instance_gaussian_index);
    gfxDestroyBuffer(gfx, buf_.active_gaussian_instance_gaussian_index_sorted);
    gfxDestroyBuffer(gfx, buf_.tile_gaussian_instance_start);
    gfxDestroyBuffer(gfx, buf_.tile_gaussian_instance_end);
    gfxDestroyBuffer(gfx, buf_.ray_to_trace_count);
    gfxDestroyBuffer(gfx, buf_.ray_to_trace_direction);
    gfxDestroyBuffer(gfx, buf_.ray_to_trace_origin);
    gfxDestroyBuffer(gfx, buf_.ray_to_trace_t_max);
    gfxDestroyBuffer(gfx, buf_.ray_to_trace_flags);
    gfxDestroyBuffer(gfx, buf_.ray_to_trace_result);
    gfxDestroyBuffer(gfx, buf_.UB);
    gfxDestroyTexture(gfx, tex_.G_color);
//    gfxDestroyTexture(gfx, tex_.output);
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
    }

    auto defines_c = std::make_unique<const char*[]>(defines.size());
    for(int i = 0; i < defines.size(); i++) {
        defines_c[i] = defines[i].c_str();
    }
    int define_count = defines.size();

    // Compute kernels

    {
        kernel_.GenerateRTMesh = gfxCreateComputeKernel(gfx, program_, "GenerateRTMesh", defines_c.get(), define_count);

        kernel_.GenerateDispatchIndirect = gfxCreateComputeKernel(gfx, program_, "GenerateDispatchIndirect",
                                                                  defines_c.get(), define_count);

        kernel_.ClearCounters = gfxCreateComputeKernel(gfx, program_, "ClearCounters", defines_c.get(), define_count);
        kernel_.TransformAndSplatGaussians = gfxCreateComputeKernel(gfx, program_, "TransformAndSplatGaussians",
                                                                    defines_c.get(), define_count);
        kernel_.ShadeActiveGaussians = gfxCreateComputeKernel(gfx, program_, "ShadeActiveGaussians", defines_c.get(),
                                                              define_count);
        kernel_.SetActiveGaussianInstanceCount = gfxCreateComputeKernel(gfx, program_, "SetActiveGaussianInstanceCount",
                                                                        defines_c.get(), define_count);
        kernel_.AssignGaussianInstanceKeys = gfxCreateComputeKernel(gfx, program_, "AssignGaussianInstanceKeys",
                                                                    defines_c.get(), define_count);
        kernel_.FindTileGaussianInstanceStarts = gfxCreateComputeKernel(gfx, program_, "FindTileGaussianInstanceStarts",
                                                                        defines_c.get(), define_count);
        kernel_.RasterizeActiveGaussians = gfxCreateComputeKernel(gfx, program_, "RasterizeActiveGaussians",
                                                                  defines_c.get(), define_count);

        kernel_.SpawnCameraRays = gfxCreateComputeKernel(gfx, program_, "SpawnCameraRays", defines_c.get(),
                                                         define_count);
        kernel_.DisplayCameraRays = gfxCreateComputeKernel(gfx, program_, "DisplayCameraRays", defines_c.get(),
                                                           define_count);
    }

    // Raytracing kernels
    {
        std::vector<char const *> base_subobjects;
        base_subobjects.push_back("ShaderConfig");
        base_subobjects.push_back("PipelineConfig");

        std::vector<char const *> TraceScheduledRays_kernel_exports;
        TraceScheduledRays_kernel_exports.push_back("Trace3DGSRaygen");
        TraceScheduledRays_kernel_exports.push_back("Trace3DGSMiss");
        TraceScheduledRays_kernel_exports.push_back("Trace3DGSShadowMiss");
        TraceScheduledRays_kernel_exports.push_back("Trace3DGSAnyHit");
        TraceScheduledRays_kernel_exports.push_back("Trace3DGSShadowAnyHit");
        std::vector<char const *> TraceScheduledRays_kernel_subobjects = base_subobjects;
        TraceScheduledRays_kernel_subobjects.push_back("Trace3DGSHitGroup");
        TraceScheduledRays_kernel_subobjects.push_back("Trace3DGSHitGroupShadow");
        kernel_.Trace3DGSRays = gfxCreateRaytracingKernel(gfx, program_, nullptr, 0,
               TraceScheduledRays_kernel_exports.data(), (uint32_t)TraceScheduledRays_kernel_exports.size(),
               TraceScheduledRays_kernel_subobjects.data(), (uint32_t)TraceScheduledRays_kernel_subobjects.size(),
               defines_c.get(), define_count);

        uint32_t entry_count[kGfxShaderGroupType_Count] {
                1,
                2, // SHADING RAY / SHADOW RAY
                2,
                1 // Actually we have no callables... but leave 1 here.
        };
        GfxKernel sbt_kernels[] {kernel_.Trace3DGSRays};
        sbt_ = gfxCreateSbt(gfx, sbt_kernels, ARRAYSIZE(sbt_kernels), entry_count);

        gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Raygen, 0, "Trace3DGSRaygen");
        gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Miss, 0,   "Trace3DGSMiss");
        gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Miss, 1,   "Trace3DGSShadowMiss");
        gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Hit, 0,    "Trace3DGSHitGroup");
        gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Hit, 1,    "Trace3DGSHitGroupShadow");

    }

    // Graphics kernels

    {
        GfxDrawState draw_state = {};
        gfxDrawStateSetDepthStencilTarget(draw_state, DXGI_FORMAT_UNKNOWN);
        gfxDrawStateSetBlendMode(draw_state,
                                 D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
                                 D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_DEST_ALPHA, D3D12_BLEND_OP_ADD);
        gfxDrawStateEnableAlphaBlending(draw_state);
        kernel_.DrawActiveGaussians = gfxCreateGraphicsKernel(
                gfx, program_, draw_state, "DrawActiveGaussians", defines_c.get(), define_count);
    }
    {
        kernel_.TonemapAndDraw = gfxCreateGraphicsKernel(
                gfx, program_, "TonemapAndDraw", defines_c.get(), define_count);
    }

    return true;
}

void Renderer::DestroyKernels () {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    gfxDestroyKernel(gfx, kernel_.GenerateRTMesh);

    gfxDestroyKernel(gfx, kernel_.GenerateDispatchIndirect);

    gfxDestroyKernel(gfx, kernel_.ClearCounters);
    gfxDestroyKernel(gfx, kernel_.TransformAndSplatGaussians);
    gfxDestroyKernel(gfx, kernel_.ShadeActiveGaussians);
    gfxDestroyKernel(gfx, kernel_.SetActiveGaussianInstanceCount);
    gfxDestroyKernel(gfx, kernel_.AssignGaussianInstanceKeys);
    gfxDestroyKernel(gfx, kernel_.FindTileGaussianInstanceStarts);
    gfxDestroyKernel(gfx, kernel_.RasterizeActiveGaussians);

    gfxDestroyKernel(gfx, kernel_.Trace3DGSRays);
    gfxDestroyKernel(gfx, kernel_.SpawnCameraRays);
    gfxDestroyKernel(gfx, kernel_.DisplayCameraRays);

    gfxDestroyKernel(gfx, kernel_.DrawActiveGaussians);
    gfxDestroyKernel(gfx, kernel_.TonemapAndDraw);

    gfxDestroyProgram(gfx, program_);
}


bool Renderer::Initialize () {
    // Reset flags and counters
    should_build_acceleration_structure_ = true;
    frame_index_ = 0;

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
}
/*
 * Created: 2024/11/9
 * Author:  hineven
 * See LICENSE for licensing.
 */
#include <glm/glm.hpp>
#include <d3d12.h>
#include "renderer.h"
#include "device_scene.h"
#include "shaders/3dgs_shared.hlsl"

Renderer::Renderer () : Timed("Renderer") {

}

Renderer::~Renderer () {

}

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

    kernel_.GenerateRTMesh = gfxCreateComputeKernel(gfx, program_, "GenerateRTMesh", defines_c.get(), define_count);

    kernel_.GenerateDispatchIndirect = gfxCreateComputeKernel(gfx, program_, "GenerateDispatchIndirect", defines_c.get(), define_count);

    kernel_.ClearCounters = gfxCreateComputeKernel(gfx, program_, "ClearCounters", defines_c.get(), define_count);
    kernel_.TransformAndSplatGaussians = gfxCreateComputeKernel(gfx, program_, "TransformAndSplatGaussians", defines_c.get(), define_count);
    kernel_.ShadeActiveGaussians = gfxCreateComputeKernel(gfx, program_, "ShadeActiveGaussians", defines_c.get(), define_count);
    kernel_.SetActiveGaussianInstanceCount = gfxCreateComputeKernel(gfx, program_, "SetActiveGaussianInstanceCount", defines_c.get(), define_count);
    kernel_.AssignGaussianInstanceKeys = gfxCreateComputeKernel(gfx, program_, "AssignGaussianInstanceKeys", defines_c.get(), define_count);
    kernel_.FindTileGaussianInstanceStarts = gfxCreateComputeKernel(gfx, program_, "FindTileGaussianInstanceStarts", defines_c.get(), define_count);
    kernel_.RasterizeActiveGaussians = gfxCreateComputeKernel(gfx, program_, "RasterizeActiveGaussians", defines_c.get(), define_count);

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
        TraceScheduledRays_kernel_exports.push_back("Trace3DGSClosestHit");
        TraceScheduledRays_kernel_exports.push_back("Trace3DGSShadowHit");
        std::vector<char const *> TraceScheduledRays_kernel_subobjects = base_subobjects;
        TraceScheduledRays_kernel_subobjects.push_back("Trace3DGSHitGroup");
        kernel_.TraceScheduledRays = gfxCreateRaytracingKernel(gfx, program_, nullptr, 0,
                   TraceScheduledRays_kernel_exports.data(), (uint32_t)TraceScheduledRays_kernel_exports.size(),
                   TraceScheduledRays_kernel_subobjects.data(), (uint32_t)TraceScheduledRays_kernel_subobjects.size());

        uint32_t entry_count[kGfxShaderGroupType_Count] {
                1,
                2, // SHADING RAY / SHADOW RAY
                2,
                1
        };
        GfxKernel sbt_kernels[] {kernel_.TraceScheduledRays};
        sbt_ = gfxCreateSbt(gfx, sbt_kernels, ARRAYSIZE(sbt_kernels), entry_count);

        gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Raygen, 0, "Trace3DGSRaygen");
        gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Miss, 0,   "Trace3DGSMiss");
        gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Miss, 1,   "Trace3DGSShadowMiss");
        gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Hit, 0,    "Trace3DGSHitGroup");
        gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Hit, 1,    "Trace3DGSHitGroupShadow");
    }


    {
        GfxDrawState draw_state = {};
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

    gfxDestroyKernel(gfx, kernel_.TonemapAndDraw);

    gfxDestroyProgram(gfx, program_);
}


bool Renderer::Initialize () {
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

void Renderer::GenerateDispatchIndirect (const GfxBuffer & thread_count_buffer) {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    gfxProgramSetParameter(gfx, program_, "g_ThreadsToDispatchCountBuffer", thread_count_buffer);
    gfxCommandBindKernel(gfx, kernel_.GenerateDispatchIndirect);
    gfxCommandDispatch(gfx, 1, 1, 1);
}

void Renderer::Render() {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    auto & scene = AppInternal::GetInstance().GetScene();
    auto & camera = scene.GetCamera();

    UniformBlock UB = {};
    {
        UB.View = camera.GetViewMatrix();
        UB.Projection = camera.GetProjectionMatrix();
        UB.ViewProjection = UB.Projection * UB.View;

        UB.CameraPosition = camera.position;

        UB.NumGaussians     = scene.GetNumGaussians();

        glm::ivec2 resolution = {
                AppInternal::GetInstance().GetWindowWidth(),
                AppInternal::GetInstance().GetWindowHeight()
        };
        auto TanFieldOfView = float(tan(camera.fov_y / 2.0) * 2.0);
        UB.Focal = glm::vec2(resolution) / TanFieldOfView;
        auto Aspect = float(double(resolution.x) / resolution.y);
        UB.FieldOfView = glm::vec2(Aspect * TanFieldOfView, TanFieldOfView);
        UB.NearPlane = camera.near;
        UB.FarPlane  = camera.far;

        UB.ScreenDimensions = resolution;
        UB.TileDimensions   = resolution / TILE_SIZE;
        assert(UB.TileDimensions.x * TILE_SIZE == resolution.x);
        assert(UB.TileDimensions.y * TILE_SIZE == resolution.y);
        UB.IndirectThreadGroupSize = cfg_.wave_lane_count;

    }
    gfxBufferGetData<UniformBlock>(gfx, buf_.UB)[frame_index_ & 1] = UB;
    gfxProgramSetParameter(gfx, program_, "UB", buf_.UB);

    auto & device_scene = scene.GetDeviceScene();
    device_scene.Bind(program_);

    gfxProgramSetParameter(gfx, program_, "g_RWDispatchIndirectCommandBuffer", buf_.dispatch_indirect_command);

    gfxProgramSetParameter(gfx, program_, "g_RWGaussianActiveCountBuffer", buf_.gaussian_active_count);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianListBuffer", buf_.active_gaussian_list);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianDepthBuffer", buf_.active_gaussian_depth);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianScreenPositionBuffer", buf_.active_gaussian_screen_position);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianScreenRadiusBuffer", buf_.active_gaussian_screen_radius);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianConicWBuffer", buf_.active_gaussian_conic_w);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianTileCountBuffer", buf_.active_gaussian_tile_count);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianInstanceBaseBuffer", buf_.active_gaussian_instance_base);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianInstanceCountBuffer", buf_.active_gaussian_instance_count);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianColorBuffer", buf_.active_gaussian_color);

    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianInstanceKeyBuffer", buf_.active_gaussian_instance_key);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianInstanceKeySortedBuffer", buf_.active_gaussian_instance_key_sorted);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianInstanceGaussianIndexBuffer", buf_.active_gaussian_instance_gaussian_index);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianInstanceGaussianIndexSortedBuffer", buf_.active_gaussian_instance_gaussian_index_sorted);

    gfxProgramSetParameter(gfx, program_, "g_RWTileGaussianInstanceStartBuffer", buf_.tile_gaussian_instance_start);
    gfxProgramSetParameter(gfx, program_, "g_RWTileGaussianInstanceEndBuffer", buf_.tile_gaussian_instance_end);

    gfxProgramSetParameter(gfx, program_, "g_RW_GColorTexture", tex_.G_color);
    gfxProgramSetParameter(gfx, program_, "g_GColorTexture", tex_.G_color);

    auto & samplers = AppInternal::GetInstance().GetSamplers();
    gfxProgramSetParameter(gfx, program_, "g_LinearClampSampler", samplers.linear_clamp);
    gfxProgramSetParameter(gfx, program_, "g_LinearWrapSampler", samplers.linear_wrap);

    // Rendering begins

    // Build the acceleration structure if required
    if(should_build_acceleration_structure_) {
        std::cout << "Building acceleration structure" << std::endl;
        if(!device_scene.acceleration_structure_) {
            device_scene.acceleration_structure_ = gfxCreateAccelerationStructure(gfx);
        }
        GfxBuffer vertex_buffer = gfxCreateBuffer<glm::vec3>(gfx, 12 * scene.GetNumGaussians());
        GfxBuffer index_buffer = gfxCreateBuffer<int>(gfx, 60 * scene.GetNumGaussians());

        gfxProgramSetParameter(gfx, program_, "g_RW_RTVertexBuffer", vertex_buffer);
        gfxProgramSetParameter(gfx, program_, "g_RW_RTIndexBuffer", index_buffer);

        auto timed_section = TimedSection(*this, "GenerateRTMesh");
        gfxCommandBindKernel(gfx, kernel_.GenerateRTMesh);
        auto num_threads = gfxKernelGetNumThreads(gfx, kernel_.GenerateRTMesh);
        gfxCommandDispatch(gfx, divideAndRoundUp(scene.GetNumGaussians(), (int)num_threads[0]), 1, 1);

        // Wait for the command to finish
        gfxFinish(gfx);

        if(!device_scene.rt_primitive_) {
            device_scene.rt_primitive_ = gfxCreateRaytracingPrimitive(gfx, device_scene.acceleration_structure_);
        }
        gfxRaytracingPrimitiveBuild(
                gfx, device_scene.rt_primitive_, index_buffer, vertex_buffer, sizeof(glm::vec3)
        );
        auto identity = glm::mat4(1.0f);
        gfxRaytracingPrimitiveSetTransform(gfx, device_scene.rt_primitive_, &identity[0][0]);
        gfxRaytracingPrimitiveSetInstanceID(gfx, device_scene.rt_primitive_, 0);
        gfxRaytracingPrimitiveSetInstanceContributionToHitGroupIndex(
                gfx, device_scene.rt_primitive_,
                0
        );
        gfxAccelerationStructureUpdate(gfx, device_scene.acceleration_structure_);

        gfxDestroyBuffer(gfx, vertex_buffer);
        gfxDestroyBuffer(gfx, index_buffer);

        should_build_acceleration_structure_ = false;
    }

    {
        auto section = TimedSection(*this, "ClearTextures");
        gfxCommandClearTexture(gfx, tex_.G_color);
    }

    {
        auto section = TimedSection(*this, "ClearCounters");
        gfxCommandBindKernel(gfx, kernel_.ClearCounters);
        gfxCommandDispatch(gfx, 1, 1, 1);
    }

    // Transform and first cull all gaussians outside of the view frustum
    {
        auto section = TimedSection(*this, "TransformAndSplatGaussians");
        gfxCommandBindKernel(gfx, kernel_.TransformAndSplatGaussians);
        auto num_threads = gfxKernelGetNumThreads(gfx, kernel_.TransformAndSplatGaussians);
        gfxCommandDispatch(gfx, divideAndRoundUp(scene.GetNumGaussians(), (int)num_threads[0]), 1, 1);
    }

    // Compute the color for each gaussian with the current view direction using SH
    {
        auto section = TimedSection(*this, "ShadeActiveGaussians");
        GenerateDispatchIndirect(buf_.gaussian_active_count);
        gfxCommandBindKernel(gfx, kernel_.ShadeActiveGaussians);
        gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
    }

    // Scan sum all tiles to get total number of gaussian instances.
    // A gaussian instance is spawned per each unique overlapping tile-gaussian pair.
    {
        auto section = TimedSection(*this, "ScanSumTileGaussianInstances");
        gfxCommandScanSum(gfx, kGfxDataType_Uint,
                          buf_.active_gaussian_instance_base,
                          buf_.active_gaussian_tile_count,
                          &buf_.gaussian_active_count);
    }

    // Sum the number of active gaussian instances into a counter buffer for further processing.
    {
        auto section = TimedSection(*this, "SetActiveGaussianInstanceCount");
        gfxCommandBindKernel(gfx, kernel_.SetActiveGaussianInstanceCount);
        gfxCommandDispatch(gfx, 1, 1, 1);
    }

    // Assign a sort key to each active gaussian instance.
    {
        auto section = TimedSection(*this, "AssignGaussianInstanceKeys");
        GenerateDispatchIndirect(buf_.active_gaussian_instance_count);
        gfxCommandBindKernel(gfx, kernel_.AssignGaussianInstanceKeys);
        gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
    }

    // Sort the active gaussian instances by their sort key.
    {
        auto section = TimedSection(*this, "SortActiveGaussianInstances");
//        gfxCommandBindKernel(gfx, kernel_.SetRadixSortDispatchParams);
//        gfxCommandBindKernel(gfx, kernel_.RadixSort);
        gfxCommandSortRadix(gfx,
                            buf_.active_gaussian_instance_key_sorted,
                            buf_.active_gaussian_instance_key,
                            &buf_.active_gaussian_instance_gaussian_index_sorted,
                            &buf_.active_gaussian_instance_gaussian_index,
                            &buf_.active_gaussian_instance_count);
    }

    {
        auto section = TimedSection(*this, "ClearTileGaussianInstanceStartsEnds");
        gfxCommandClearBuffer(gfx, buf_.tile_gaussian_instance_start, 0xffffffffu);
        gfxCommandClearBuffer(gfx, buf_.tile_gaussian_instance_end, 0xffffffffu);
    }

    {
        assert(DEFAULT_REPEAT == 1);
        auto section = TimedSection(*this, "FindTileGaussianInstanceStarts");
        gfxCommandBindKernel(gfx, kernel_.FindTileGaussianInstanceStarts);
        gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
    }

    {
        auto section = TimedSection(*this, "RasterizeActiveGaussians");
        gfxCommandBindKernel(gfx, kernel_.RasterizeActiveGaussians);
        int tile_count = UB.TileDimensions.x * UB.TileDimensions.y;
        gfxCommandDispatch(gfx, tile_count, 1, 1);
    }

    {
        auto section = TimedSection(*this, "TonemapAndDraw");
        gfxCommandBindKernel(gfx, kernel_.TonemapAndDraw);
        gfxCommandDraw(gfx, 3, 1);
    }

    frame_index_ ++;
}
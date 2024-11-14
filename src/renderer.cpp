/*
 * Created: 2024/11/9
 * Author:  hineven
 * See LICENSE for licensing.
 */
#include <glm/glm.hpp>
#include <d3d12.h>
#include "gfx_imgui.h"
#include "renderer.h"
#include "device_scene.h"
#include "shaders/3dgs_shared.hlsl"

Renderer::Renderer () : Timed("Renderer") {

}

Renderer::~Renderer () {

}

void Renderer::GenerateDispatchIndirect (const GfxBuffer & thread_count_buffer) {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    gfxProgramSetParameter(gfx, program_, "g_ThreadsToDispatchCountBuffer", thread_count_buffer);
    gfxCommandBindKernel(gfx, kernel_.GenerateDispatchIndirect);
    gfxCommandDispatch(gfx, 1, 1, 1);
}

void Renderer::RenderUI () {
    if(ImGui::CollapsingHeader("Renderer")) {
        ImGui::Checkbox("Show HWRT Color", &options_.show_HWRT_color);
        ImGui::SliderFloat("Gaussian RT Proxy Geometry Sigma", &options_.gaussian_RT_proxy_geometry_sigma, 0.01f, 1.0f);
        ImGui::SliderFloat("Min Alpha For Gaussian Evaluation", &options_.min_alpha_for_gaussian_evaluation, 0.0f, 0.5f);
    }
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
        auto tan_fov_y = tan(camera.fov_y / 2.0);
        auto two_tan_fov_y = float(tan_fov_y * 2.0);
        UB.CameraFocal = glm::vec2(resolution) / two_tan_fov_y;
        auto Aspect = float(double(resolution.x) / resolution.y);
        UB.CameraFieldOfView = glm::vec2(Aspect * two_tan_fov_y, two_tan_fov_y);

        glm::vec3 axis_forward = camera.direction;
        // Camera forward is -z axis
        glm::vec3 axis_right = glm::normalize(glm::cross(axis_forward, camera.up));
        glm::vec3 axis_up = glm::normalize(glm::cross(axis_right, axis_forward));
        // Thus, normalize(axis_forward + axis_right * ndc.x + axis_up * ndc.y) is the camera ray direction
        axis_up    *= tan_fov_y;
        axis_right *= tan_fov_y * Aspect;

        UB.CameraRight = axis_right;
        UB.CameraNearPlane = camera.near;

        UB.CameraUp = axis_up;
        UB.CameraFarPlane  = camera.far;

        UB.CameraDirection = axis_forward;
        UB.GaussianRTProxyGeometrySigma = options_.gaussian_RT_proxy_geometry_sigma;

        UB.ScreenDimensions = resolution;
        UB.TileDimensions   = resolution / TILE_SIZE;

        assert(UB.TileDimensions.x * TILE_SIZE == resolution.x);
        assert(UB.TileDimensions.y * TILE_SIZE == resolution.y);
        UB.IndirectThreadGroupSize = cfg_.wave_lane_count;
        UB.MinAlphaForGaussianEvaluation = options_.min_alpha_for_gaussian_evaluation;
        UB.SmallTileDimensions = resolution / SMALL_TILE_SIZE;
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

    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceCountBuffer", buf_.ray_to_trace_count);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceDirectionBuffer", buf_.ray_to_trace_direction);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceOriginBuffer", buf_.ray_to_trace_origin);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceTMaxBuffer", buf_.ray_to_trace_t_max);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceFlagsBuffer", buf_.ray_to_trace_flags);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceResultBuffer", buf_.ray_to_trace_result);

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

        for(auto primitive : device_scene.rt_primitives_) {
            gfxDestroyRaytracingPrimitive(gfx, primitive);
        }
        device_scene.rt_primitives_.resize(scene.GetNumInstances());

        for(int i = 0; i < (int)scene.GetNumInstances(); i++) {
            device_scene.rt_primitives_[i] = gfxCreateRaytracingPrimitive(gfx, device_scene.acceleration_structure_);
            auto index_range = gfxCreateBufferRange(
                    gfx, index_buffer, scene.gsi_gs_index_offsets_[i] * 60 * sizeof(int),
                    scene.gsi_gs_counts_[i] * 60 * sizeof(int));
            auto vertex_range = gfxCreateBufferRange(
                    gfx, vertex_buffer, scene.gsi_gs_index_offsets_[i] * 12 * sizeof(glm::vec3),
                    scene.gsi_gs_counts_[i] * 12 * sizeof(glm::vec3));
            gfxRaytracingPrimitiveBuild(
                    gfx, device_scene.rt_primitives_[i], index_range, vertex_range, sizeof(glm::vec3)
            );
            glm::mat4x3 mat4x3_colmajor = scene.gsi_transforms_[i];
            float mat4x4_rowmajor[16] = {
                    mat4x3_colmajor[0][0], mat4x3_colmajor[1][0], mat4x3_colmajor[2][0], mat4x3_colmajor[3][0],
                    mat4x3_colmajor[0][1], mat4x3_colmajor[1][1], mat4x3_colmajor[2][1], mat4x3_colmajor[3][1],
                    mat4x3_colmajor[0][2], mat4x3_colmajor[1][2], mat4x3_colmajor[2][2], mat4x3_colmajor[3][2],
                    0, 0, 0, 1
                };
            gfxRaytracingPrimitiveSetTransform(gfx, device_scene.rt_primitives_[i], mat4x4_rowmajor);
            gfxRaytracingPrimitiveSetInstanceID(gfx, device_scene.rt_primitives_[i], i);
            gfxRaytracingPrimitiveSetInstanceContributionToHitGroupIndex(
                    gfx, device_scene.rt_primitives_[i],
                    0
            );
            gfxDestroyBuffer(gfx, index_range);
            gfxDestroyBuffer(gfx, vertex_range);
        }
        gfxAccelerationStructureUpdate(gfx, device_scene.acceleration_structure_);

        gfxDestroyBuffer(gfx, vertex_buffer);
        gfxDestroyBuffer(gfx, index_buffer);

        should_build_acceleration_structure_ = false;
    }

    // Bind the acceleration structure if present
    if(device_scene.acceleration_structure_) {
        gfxProgramSetParameter(gfx, program_, "g_HWRT_AccelerationStructure", device_scene.acceleration_structure_);
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

    if(options_.show_HWRT_color) {
        // HWRT pipeline

        auto section = TimedSection(*this, "HWRT Color");

        gfxCommandBindKernel(gfx, kernel_.SpawnCameraRays);
        // Rays are packed in small tiles.
        int num_groups = UB.SmallTileDimensions.x * UB.SmallTileDimensions.y;
        gfxCommandDispatch(gfx, num_groups, 1, 1);

        int num_rays = num_groups * TILE_SIZE * TILE_SIZE;
        gfxCommandBindKernel(gfx, kernel_.Trace3DGSRays);
        gfxCommandDispatchRays(gfx, sbt_, num_rays, 1, 1);

        gfxCommandBindKernel(gfx, kernel_.DisplayCameraRays);
        gfxCommandDispatch(gfx, num_groups, 1, 1);

    } else {
        // Rasterization pipeline

        // Transform and first cull all gaussians outside of the view frustum
        {
            auto section = TimedSection(*this, "TransformAndSplatGaussians");
            gfxCommandBindKernel(gfx, kernel_.TransformAndSplatGaussians);
            auto num_threads = gfxKernelGetNumThreads(gfx, kernel_.TransformAndSplatGaussians);
            gfxCommandDispatch(gfx, divideAndRoundUp(scene.GetNumGaussians(), (int) num_threads[0]), 1, 1);
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
    }

    {
        auto section = TimedSection(*this, "TonemapAndDraw");
        gfxCommandBindKernel(gfx, kernel_.TonemapAndDraw);
        gfxCommandDraw(gfx, 3, 1);
    }

    frame_index_ ++;
}
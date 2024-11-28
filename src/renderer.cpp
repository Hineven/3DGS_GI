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
#include "3dgs_shared.hlsl"

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

void Renderer::GenerateDrawIndirect(const GfxBuffer &vertex_count_buffer) {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    gfxProgramSetParameter(gfx, program_, "g_VertexToDrawCountBuffer", vertex_count_buffer);
    gfxCommandBindKernel(gfx, kernel_.GenerateDrawIndirect);
    gfxCommandDispatch(gfx, 1, 1, 1);
}

void Renderer::RenderUI () {
    if(ImGui::CollapsingHeader("Renderer")) {
        ImGui::Checkbox("Show HWRT Color", &options_.visualize_HWRT);
        ImGui::SliderFloat("Gaussian RT Proxy Geometry Sigma", &options_.gaussian_RT_proxy_geometry_sigma, 0.01f, 1.0f);
        ImGui::SliderFloat("Min Alpha For Gaussian Evaluation", &options_.min_alpha_for_gaussian_evaluation, 0.0f, 0.5f);
        const char * debug_modes[] = {
            "Default",
            "Albedo/Color",
            "Roughness",
            "Normal",
            "Momentum",
            "Alpha"
        };
        if (ImGui::BeginCombo("DebugMode", debug_modes[options_.debug_mode])) {
            for (int i = 0; i < IM_ARRAYSIZE(debug_modes); i++) {
                if (ImGui::Selectable(debug_modes[i])) {
                    options_.debug_mode = i;
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::Checkbox("Render Color Only", &options_.no_G_buffers)) {
            need_reload_shaders_ = true;
        }
        if (ImGui::Checkbox("Reconstruct Normals", &options_.reconstruct_normals)) {
            need_reload_shaders_ = true;
        }
    }
}

void Renderer::Render() {

    if (need_reload_shaders_) {
        Destroy();
        if (!Initialize()) {
            return;
        }
        need_reload_shaders_ = false;
    }

    auto & gfx = AppInternal::GetInstance().GetGfx();
    auto & scene = AppInternal::GetInstance().GetScene();
    auto & camera = scene.GetCamera();

    UniformBlock UB = {};
    {
        glm::ivec2 resolution = {
                AppInternal::GetInstance().GetWindowWidth(),
                AppInternal::GetInstance().GetWindowHeight()
        };
        UB.MainCamera = camera.PackDescription(resolution.x, resolution.y);

        UB.NumGaussians     = scene.GetNumGaussians();
        UB.GaussianRTProxyGeometrySigma = options_.gaussian_RT_proxy_geometry_sigma;
        UB.IndirectThreadGroupSize = cfg_.wave_lane_count;
        UB.MinAlphaForGaussianEvaluation = options_.min_alpha_for_gaussian_evaluation;

        UB.ScreenDimensions = resolution;
        UB.TileDimensions   = resolution / TILE_SIZE;
        assert(UB.TileDimensions.x * TILE_SIZE == resolution.x);
        assert(UB.TileDimensions.y * TILE_SIZE == resolution.y);

        UB.SmallTileDimensions = resolution / SMALL_TILE_SIZE;
        UB.RT_AlphaMultiplier = 2.f;
        UB.FrameIndex = frame_index_;

        UB.DebugMode = options_.debug_mode;
        UB.VisualizeShadingRays = false;
    }
    gfxBufferGetData<UniformBlock>(gfx, buf_.UB)[frame_index_ & 1] = UB;
    gfxProgramSetParameter(gfx, program_, "UB", buf_.UB);

    auto & device_scene = scene.GetDeviceScene();
    device_scene.Bind(program_);

    gfxProgramSetParameter(gfx, program_, "g_RWDispatchIndirectCommandBuffer", buf_.dispatch_indirect_command);
    gfxProgramSetParameter(gfx, program_, "g_RWDispatchRaysIndirectCommandBuffer", buf_.dispatch_rays_indirect_command);
    gfxProgramSetParameter(gfx, program_, "g_RWDrawIndirectCommandBuffer", buf_.draw_indirect_command);

    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianCountBuffer", buf_.active_gaussian_count);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianListSrcBuffer", buf_.active_gaussian_list_src);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianListBuffer", buf_.active_gaussian_list);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianLinearDepthSrcBuffer", buf_.active_gaussian_linear_depth_src);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianLinearDepthBuffer", buf_.active_gaussian_linear_depth);

    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianNDCPositionBuffer", buf_.active_gaussian_NDC_position);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianQuadNDCVector0Buffer", buf_.active_gaussian_quad_NDC_vector0);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianQuadNDCVector1Buffer", buf_.active_gaussian_quad_NDC_vector1);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianColorBuffer", buf_.active_gaussian_color);

    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceCountBuffer", buf_.ray_to_trace_count);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceDirectionBuffer", buf_.ray_to_trace_direction);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceOriginBuffer", buf_.ray_to_trace_origin);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceTMaxBuffer", buf_.ray_to_trace_t_max);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceFlagsBuffer", buf_.ray_to_trace_flags);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceResultBuffer", buf_.ray_to_trace_result);

    gfxProgramSetParameter(gfx, program_, "g_RW_GColorTexture", tex_.G_albedo_alpha);
    gfxProgramSetParameter(gfx, program_, "g_GColorTexture", tex_.G_albedo_alpha);
    gfxProgramSetParameter(gfx, program_, "g_RW_GMomentumTexture", tex_.G_momentum);
    gfxProgramSetParameter(gfx, program_, "g_GMomentumTexture", tex_.G_momentum);
    gfxProgramSetParameter(gfx, program_, "g_RW_GMaterialTexture", tex_.G_material);
    gfxProgramSetParameter(gfx, program_, "g_GMaterialTexture", tex_.G_material);
    // Not rasterized, but derived from depth buffer (overdraw is too severe for 3dgs)
    gfxProgramSetParameter(gfx, program_, "g_RW_GNormalTexture", tex_.G_normal);
    gfxProgramSetParameter(gfx, program_, "g_GNormalTexture", tex_.G_normal);

    gfxProgramSetParameter(gfx, program_, "g_RW_Radiance", tex_.radiance);
    gfxProgramSetParameter(gfx, program_, "g_Radiance", tex_.radiance);

    auto & samplers = AppInternal::GetInstance().GetSamplers();
    gfxProgramSetParameter(gfx, program_, "g_LinearClampSampler", samplers.linear_clamp);
    gfxProgramSetParameter(gfx, program_, "g_LinearWrapSampler", samplers.linear_wrap);

    // Rendering begins

    // Build the acceleration structure if required
    if(should_build_acceleration_structure_) {
        std::cout << "Building acceleration structure" << std::endl;
        if(!device_scene.acceleration_structure_) {
            device_scene.acceleration_structure_ = gfxCreateAccelerationStructure(gfx);
        } else {
            for(auto primitive : device_scene.rt_primitives_) {
                gfxDestroyRaytracingPrimitive(gfx, primitive);
            }
            device_scene.rt_primitives_.clear();
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
        gfxCommandClearTexture(gfx, tex_.G_albedo_alpha);
    }

    {
        auto section = TimedSection(*this, "ClearCounters");
        gfxCommandBindKernel(gfx, kernel_.ClearCounters);
        gfxCommandDispatch(gfx, 1, 1, 1);
    }

    if(options_.visualize_HWRT) {
        // HWRT pipeline

        auto section = TimedSection(*this, "HWRT Color");

        gfxCommandBindKernel(gfx, kernel_.SpawnCameraRays);
        // Rays are packed in small tiles.
        int num_groups = UB.SmallTileDimensions.x * UB.SmallTileDimensions.y;
        gfxCommandDispatch(gfx, num_groups, 1, 1);

        int num_rays = num_groups * TILE_SIZE * TILE_SIZE;
        gfxCommandBindKernel(gfx, kernel_.Trace3DGSRays);
        gfxCommandDispatchRays(gfx, sbt_, num_rays, 1, 1);

    } else {
        // Rasterization pipeline
        {
            // Filter active gaussians, crop gaussians outside the view frustrum
            auto section = TimedSection(*this, "FilterActiveGaussians");
            gfxCommandBindKernel(gfx, kernel_.FilterActiveGaussians);
            auto num_threads = gfxKernelGetNumThreads(gfx, kernel_.FilterActiveGaussians);
            uint num_groups = divideAndRoundUp((uint)scene.GetNumGaussians(), num_threads[0]);
            gfxCommandDispatch(gfx, num_groups, 1, 1);
        }

        {
            // Sort active gaussians according to their depth values.
            // So we can later draw them in order.
            auto section = TimedSection(*this, "SortActiveGaussians");
            gfxCommandSortRadix(gfx, buf_.active_gaussian_linear_depth, buf_.active_gaussian_linear_depth_src,
                                &buf_.active_gaussian_list, &buf_.active_gaussian_list_src, &buf_.active_gaussian_count);
        }

        {
            // Project active gaussians, compute the 2-dimensional gaussian distribution
            // Save the eigen vectors for later rasterization.
            auto section = TimedSection(*this, "ProjectActiveGaussians");
            GenerateDispatchIndirect(buf_.active_gaussian_count);
            gfxCommandBindKernel(gfx, kernel_.ProjectActiveGaussians);
            gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
        }

        {
            // Rasterize the G-Buffers
            auto section = TimedSection(*this, "DrawActiveGaussians");
            // Cleared to (0, 0, 0, 0)
            gfxCommandClearTexture(gfx, tex_.G_albedo_alpha);
            gfxCommandClearTexture(gfx, tex_.G_material);
            gfxCommandClearTexture(gfx, tex_.G_momentum);
            gfxCommandClearTexture(gfx, tex_.G_normal);
            GenerateDrawIndirect(buf_.active_gaussian_count);
            gfxCommandBindKernel(gfx, kernel_.DrawActiveGaussians);
            gfxCommandBindColorTarget(gfx, 0, tex_.G_albedo_alpha);
            gfxCommandBindColorTarget(gfx, 1, tex_.G_material);
            gfxCommandBindColorTarget(gfx, 2, tex_.G_momentum);
            if (!options_.reconstruct_normals) {
                gfxCommandBindColorTarget(gfx, 3, tex_.G_normal);
            }
            gfxCommandMultiDrawIndirect(gfx, buf_.draw_indirect_command, 1);
        }

        {
            auto section = TimedSection(*this, "ResolveGBuffers");
            gfxCommandBindKernel(gfx, kernel_.ResolveGBuffers);
            auto num_threads = gfxKernelGetNumThreads(gfx, kernel_.ResolveGBuffers);
            assert(UB.ScreenDimensions.x % num_threads[0] == 0 && UB.ScreenDimensions.y % num_threads[1] == 0);
            uint num_groups_x = divideAndRoundUp((uint)UB.ScreenDimensions.x, num_threads[0]);
            uint num_groups_y = divideAndRoundUp((uint)UB.ScreenDimensions.y, num_threads[1]);
            gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
        }

        if (options_.reconstruct_normals) {
            // Reconstruct normals from depth buffer if required
            auto section = TimedSection(*this, "ReconstructNormals");
            gfxCommandBindKernel(gfx, kernel_.ReconstructNormals);
            gfxCommandDispatch(gfx, UB.TileDimensions.x, UB.TileDimensions.y, 1);
        }

    }

    if (!options_.visualize_HWRT) {
        auto section = TimedSection(*this, "FinalComposition");
        gfxCommandBindKernel(gfx, kernel_.FinalComposition);
        gfxCommandDispatch(gfx, UB.TileDimensions.x, UB.TileDimensions.y, 1);
    } else {
        int num_groups = UB.SmallTileDimensions.x * UB.SmallTileDimensions.y;
        gfxCommandBindKernel(gfx, kernel_.DisplayCameraRays);
        gfxCommandDispatch(gfx, num_groups, 1, 1);
    }

    {
        auto section = TimedSection(*this, "TonemapAndDraw");
        gfxCommandBindKernel(gfx, kernel_.TonemapAndDraw);
        gfxCommandDraw(gfx, 3, 1);
    }

    frame_index_ ++;
}
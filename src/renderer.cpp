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

// Flag for debugging. Sometimes incorrect indirect dispatches will let my system panic.
// This flag disables all the indirect shader dispatches so i can safely check for
// shader compilation errors.
// #define NO_INDIRECT_DISPATCH

Renderer::Renderer () : Timed("Renderer"), blue_noise_sampler_(AppInternal::GetInstance().GetGfx()) {

}

Renderer::~Renderer () {

}

void Renderer::GenerateDispatchRaysIndirect(const GfxBuffer &ray_count_buffer) {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    gfxProgramSetParameter(gfx, program_, "g_RaysToDispatchCountBuffer", ray_count_buffer);
    gfxCommandBindKernel(gfx, kernel_.GenerateDispatchRaysIndirect);
    gfxCommandDispatch(gfx, 1, 1, 1);
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
        ImGui::Checkbox("HWRT Visualize", &options_.visualize_HWRT);
        const char * debug_modes[] = {
            "Default",
            "Albedo/Color",
            "Roughness",
            "Normal",
            "Depth",
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
        ImGui::Checkbox("SSRT Enable", &options_.SSRT_enable);
        ImGui::Checkbox("HWRT Enable", &options_.HWRT_enable);

        for (auto & var : cvar_) {
            bool tmp_bv = false;
            auto & ref = var.second;
            switch (ref.type) {
                case CVar::BOOL:
                    tmp_bv = ref.v.i;
                    ref.v.i = ImGui::Checkbox(ref.name.c_str(), &tmp_bv);
                case CVar::FLOAT:
                    if (ref.mn == ref.mx)
                        ImGui::InputFloat(ref.name.c_str(), (float*)&ref.v);
                    else ImGui::SliderFloat(ref.name.c_str(), (float*)&ref.v, ref.mn, ref.mx);
                    break;
                case CVar::INT:
                    if (ref.mn == ref.mx) ImGui::InputInt(ref.name.c_str(), (int*)&ref.v);
                    else ImGui::SliderInt(ref.name.c_str(), (int*)&ref.v, ref.mn, ref.mx);
                case CVar::VEC2:
                    if (ref.mn == ref.mx) ImGui::InputFloat2(ref.name.c_str(), (float*)&ref.v);
                    else ImGui::SliderFloat2(ref.name.c_str(), (float*)&ref.v, ref.mn, ref.mx);
                case CVar::VEC3:
                    if (ref.mn == ref.mx) ImGui::InputFloat3(ref.name.c_str(), (float*)&ref.v);
                    else ImGui::SliderFloat3(ref.name.c_str(), (float*)&ref.v, ref.mn, ref.mx);
            }
            ImGui::SetItemTooltip("%s", ref.name.c_str());
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

    UB = {};
    {
        auto registerCVar = [&](std::string name, std::string desc, auto & var_ref, auto default_value, double mn = 0, double mx = 0) {
            using raw_t = std::remove_cvref_t<decltype(var_ref)>;
            using def_t = std::remove_cvref_t<decltype(default_value)>;
            auto it = cvar_.find(&var_ref);
            if (it != cvar_.end()) {
                var_ref = it->second[frame_index_ % 8];
            } else {
                var_ref = default_value;
                auto & cvar_ref = cvar_[var_ref];
                cvar_ref.name = name;
                cvar_ref.desc = desc;
                cvar_ref.mn = mn;
                cvar_ref.mx = mx;
                if (std::is_same_v<def_t, bool>) {
                    cvar_ref.type = CVar::BOOL;
                    cvar_ref.v.i = default_value;
                } else if (std::is_integral_v<raw_t>) {
                    cvar_ref.type = CVar::INT;
                    cvar_ref.v.i = default_value;
                } else if (std::is_convertible_v<raw_t, float>) {
                    cvar_ref.type = CVar::FLOAT;
                    cvar_ref.v.f = default_value;
                } else if (std::is_same_v<raw_t, glm::vec2>) {
                    cvar_ref.type = CVar::VEC2;
                    cvar_ref.v.f2 = default_value;
                } else if (std::is_same_v<raw_t, glm::vec3>) {
                    cvar_ref.type = CVar::VEC3;
                    cvar_ref.v.f3 = default_value;
                } else {
                    app_assert(false);
                }
            }
            auto & cvar_ref = cvar_[var_ref];
            memcpy(var_ref, cvar_ref.v, sizeof(raw_t));
        };
#define REGISTER_CVAR(var, desc, ...) registerCVar(#var, desc, var, __VA_ARGS__)
        glm::ivec2 resolution = {
                AppInternal::GetInstance().GetWindowWidth(),
                AppInternal::GetInstance().GetWindowHeight()
        };
        UB.MainCamera = camera.PackDescription(resolution.x, resolution.y, previous_UB_.MainCamera);

        UB.NumGaussians     = scene.GetNumGaussians();
        REGISTER_CVAR(UB.GaussianRTProxyGeometrySigma, "The scaling of the proxy geometry in 3DGS ray tracing."
            "The original paper says 0.3 is good.", 0.3, 0.01, 1.0);
        UB.IndirectThreadGroupSize = cfg_.wave_lane_count;
        REGISTER_CVAR(UB.HWRT_MinAlphaForGaussianEvaluation, "Minimum opacity at the ray-gayssian intersection for 3DGS to be evaluated in ray tracing."
            "Otherwise, they are ignored.", 0.01f, 0.0f, 1.0f);

        UB.ScreenDimensions = resolution;
        UB.TileDimensions   = resolution / TILE_SIZE;
        assert(UB.TileDimensions.x * TILE_SIZE == resolution.x);
        assert(UB.TileDimensions.y * TILE_SIZE == resolution.y);

        UB.SmallTileDimensions = resolution / SMALL_TILE_SIZE;
        // Unused
        UB.HWRT_AlphaMultiplier = 0;
        UB.FrameIndex = frame_index_;

        UB.DebugMode = options_.debug_mode;
        REGISTER_CVAR(UB.VisualizeShadingRays, "Visualize HWRT shading ray results. Otherwise, visualize shadow ray depth results.",
            false);
        REGISTER_CVAR(UB.OpaqueThreshold, "Pixels with alpha values higher than this threshold are considered opaque.",
            0.1f, 0.0f, 1.0f);
        // Not that useful.
        UB.DepthAlphaClipValue = 0;

        float MaxTraceDistance = camera.far;
        REGISTER_CVAR(UB.HWRT_StochasticRayTracingQuality, "The quality of stochastic ray tracing."
                                                           "Lower values bring more biased but faster results.", 0.2f, 0.0f, 1.0f);
        UB.RT_MaxTraceDistance              = MaxTraceDistance;
        REGISTER_CVAR(UB.SSRT_MaxTraceDistance, "Normally, SSRT just helps to solve near field occlusions."
        "Due to the depth bias in rasterization, SSRT in 3DGS is not as reliable as it is in regular context.",
        0.25f);
        REGISTER_CVAR(UB.SSRT_RelativeTexelThickness,
            "How thick a texel is on Z axis in the projected space when doing screen space ray tracing."
            "Thicker values may produce more artifacts but can cull more rays.", 0.005f, 0.001f, 0.01f);

        REGISTER_CVAR(UB.Debug_LightPosition, "", glm::vec3(0, 0, 0), -10, 10);
        UB.SSRT_MaxNumIterations            = 50; // Consistent with Lumen

        // Light grid
        {
            glm::vec3 mn = scene.GetBoundsMin();
            glm::vec3 mx = scene.GetBoundsMax();
            glm::vec3 points[8];
            points[0] = mn;
            points[1] = glm::vec3(mx.x, mn.y, mn.z);
            points[2] = glm::vec3(mn.x, mx.y, mn.z);
            points[3] = glm::vec3(mx.x, mx.y, mn.z);
            points[4] = glm::vec3(mn.x, mn.y, mx.z);
            points[5] = glm::vec3(mx.x, mn.y, mx.z);
            points[6] = glm::vec3(mn.x, mx.y, mx.z);
            points[7] = mx;
            float radius = 0;
            for (int i = 0; i < 8; i++) {
                auto p = abs(camera.position - points[i]);
                radius = glm::max(glm::max(p.x, p.y), p.z);
            }

            int fid_mapping[8] = {
                0, 7, 1, 6, 2, 5, 3, 4
            };
            int fid_mapped = fid_mapping[frame_index_% 8];
            glm::vec3 fw = glm::vec3(fid_mapped & 1, (bool)(fid_mapped & 2), (bool)(fid_mapped & 4));
            glm::vec3 jitter {nextFloat(), nextFloat(), nextFloat()};
            jitter = glm::vec3(0.5f) * fw + 0.5f * jitter;

            for (int i = 0; i < options_.light_grid_num_cascades; i++) {
                double cascade_radius = radius / (1 << (options_.light_grid_num_cascades - i - 1));
                double cascade_grid_size = cascade_radius / options_.light_grid_size;
                glm::dvec3 cascade_center = glm::dvec3(camera.position) + cascade_grid_size * glm::dvec3(jitter);
                glm::vec3 cascade_min = glm::vec3(cascade_center - cascade_radius);
                glm::vec3 cascade_max = glm::vec3(cascade_center + cascade_radius);
                UB.LightGrid_GridCascadeMin[i] = glm::vec4(cascade_min, 0.f);
                UB.LightGrid_GridCascadeMax[i] = glm::vec4(cascade_max, 0.f);
                if (i == 0) UB.LightGrid_GridSize = glm::vec3(cascade_grid_size);
            }
            UB.LightGrid_GridResolution = options_.light_grid_size;
            UB.LightGrid_GridResolution2 = options_.light_grid_size * options_.light_grid_size;
            UB.LightGrid_GridResolution3 = options_.light_grid_size * options_.light_grid_size * options_.light_grid_size;
            UB.LightGrid_NumGridCascades = options_.light_grid_num_cascades;
            REGISTER_CVAR(UB.LightGrid_MinLightContributionForInjection, "Minimum amount of contribution estimated from a light to any grid, "
                                                                         "for the light be injected to the grid." , 1e-4f);

            REGISTER_CVAR(UB.LightGrid_MinResampleWeightForDirectIllumiation, "Minimum resample weight for a light to be resampled when sampling from"
                                                                              "light grids for pixel shading.", 1e-4f);
            UB.LightGrid_MaxNumEntries = options_.light_grid_max_num_entries;
        }

        REGISTER_CVAR(UB.DI_OcclusionThresholdMinFactor, "Minimum factor of shadow ray length threshold upon DI occlusion tests.", 0.97f);
        REGISTER_CVAR(UB.DI_OcclusionThresholdMaxFactor, "Maximum factor of shadow ray length threshold upon DI occlusion tests.", 0.99f);

        gfxSbtGetGpuVirtualAddressRangeAndStride(gfx, sbt_,
            (D3D12_GPU_VIRTUAL_ADDRESS_RANGE *)&UB.RT_RayGenerationShaderRecord,
            (D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE *)&UB.RT_MissShaderTable,
            (D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE *)&UB.RT_HitGroupTable,
            (D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE *)&UB.RT_CallableShaderTable);

    }
    uint32_t UB_stride = roundUp((uint32_t)sizeof(UniformBlock), 256u);
    GfxBuffer UB_range = gfxCreateBufferRange(gfx, buf_.UB, UB_stride * gfxGetBackBufferIndex(gfx), sizeof(UniformBlock));
    UB_range.setStride(UB_stride);
    gfxBufferGetData<UniformBlock>(gfx, UB_range)[0] = UB;

    previous_UB_ = UB;

    auto & device_scene = scene.GetDeviceScene();
    device_scene.Bind(program_);

    blue_noise_sampler_.InstallParameters(program_);

    // Light grid
    gfxProgramSetBuffer(gfx, program_, "g_LightGrid_GridLightListAllocator", buf_.LightGrid_grid_light_list_allocator);
    gfxProgramSetBuffer(gfx, program_, "g_LightGrid_GridLightCountBuffer", buf_.LightGrid_grid_light_count);
    gfxProgramSetBuffer(gfx, program_, "g_LightGrid_GridLightListOffsetBuffer", buf_.LightGrid_grid_light_list_offset);
    gfxProgramSetBuffer(gfx, program_, "g_LightGrid_GridLightListBuffer", buf_.LightGrid_grid_light_list);

    // Common
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

    gfxProgramSetParameter(gfx, program_, "g_RWRayCountBuffer", buf_.ray_count);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceCountBuffer", buf_.ray_to_trace_count[0]);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceListBuffer", buf_.ray_to_trace_list[0]);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceDirectionBuffer", buf_.ray_to_trace_direction);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceOriginBuffer", buf_.ray_to_trace_origin);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceSeedBuffer", buf_.ray_to_trace_seed);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceFlagsBuffer", buf_.ray_to_trace_flags);

    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceResultBuffer", buf_.ray_to_trace_result);


    gfxProgramSetParameter(gfx, program_, "g_RW_GColorTexture", tex_.G_albedo_alpha);
    gfxProgramSetParameter(gfx, program_, "g_GColorTexture", tex_.G_albedo_alpha);
    gfxProgramSetParameter(gfx, program_, "g_RW_GDepthTexture", tex_.G_depth);
    gfxProgramSetParameter(gfx, program_, "g_GDepthTexture", tex_.G_depth);
    gfxProgramSetParameter(gfx, program_, "g_RW_GMaterialTexture", tex_.G_material);
    gfxProgramSetParameter(gfx, program_, "g_GMaterialTexture", tex_.G_material);
    // Not rasterized, but derived from depth buffer (overdraw is too severe for 3dgs)
    gfxProgramSetParameter(gfx, program_, "g_RW_GNormalTexture", tex_.G_normal);
    gfxProgramSetParameter(gfx, program_, "g_GNormalTexture", tex_.G_normal);
    gfxProgramSetParameter(gfx, program_, "g_RW_GFilteredDepthTexture", tex_.G_filtered_depth);
    gfxProgramSetParameter(gfx, program_, "g_GFilteredDepthTexture", tex_.G_filtered_depth);
    gfxProgramSetParameter(gfx, program_, "g_RW_GZDepthTexture", tex_.G_zdepth[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_GZDepthTexture", tex_.G_zdepth[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_PreviousZDepthTexture", tex_.G_zdepth[(frame_index_ + 1) & 1]);

    gfxProgramSetParameter(gfx, program_, "g_NearHZBTexture", tex_.near_HZB);

    gfxProgramSetParameter(gfx, program_, "g_RW_Radiance", tex_.radiance[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_Radiance", tex_.radiance[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_PreviousRadiance", tex_.radiance[(frame_index_ + 1) & 1]);

    auto & samplers = AppInternal::GetInstance().GetSamplers();
    gfxProgramSetParameter(gfx, program_, "g_LinearClampSampler", samplers.linear_clamp);
    gfxProgramSetParameter(gfx, program_, "g_LinearWrapSampler", samplers.linear_wrap);

    gfxProgramSetBuffer(gfx, program_, "UB", UB_range);

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
        // Visualize HWRT trace results
        {
            auto section = TimedSection(*this, "HWRT Trace");

            gfxCommandBindKernel(gfx, kernel_.SpawnCameraRays);
            // Rays are packed in small tiles.
            int num_groups = UB.SmallTileDimensions.x * UB.SmallTileDimensions.y;
            gfxCommandDispatch(gfx, num_groups, 1, 1);

            int num_rays = num_groups * TILE_SIZE * TILE_SIZE;
            if (UB.VisualizeShadingRays) {
                gfxCommandBindKernel(gfx, kernel_.Trace3DGSRays);
                gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Raygen, 0, "Trace3DGSRaygen");
                gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Hit, 0, "Trace3DGSHitGroup");
                gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Miss, 0, "Trace3DGSMiss");
            } else {
                gfxCommandBindKernel(gfx, kernel_.Trace3DGSShadowRays);
                gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Raygen, 0, "Trace3DGSShadowRaygen");
                gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Hit, 0, "Trace3DGSShadowHitGroup");
                gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Miss, 0, "Trace3DGSShadowMiss");
            }
            gfxCommandDispatchRays(gfx, sbt_, num_rays, 1, 1);
        }

        {
            auto section = TimedSection(*this, "DisplayCameraRays");
            int num_groups = UB.SmallTileDimensions.x * UB.SmallTileDimensions.y;
            gfxCommandBindKernel(gfx, kernel_.DisplayCameraRays);
            gfxCommandDispatch(gfx, num_groups, 1, 1);
        }
    } else {
        // Ordinary rendering stuff including the reconstruction of G-Buffers
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
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
#endif
        }

        {
            // Rasterize the G-Buffers
            auto section = TimedSection(*this, "DrawActiveGaussians");
            // Cleared to (0, 0, 0, 0)
            gfxCommandClearTexture(gfx, tex_.G_albedo_alpha);
            gfxCommandClearTexture(gfx, tex_.G_material);
            gfxCommandClearTexture(gfx, tex_.G_depth);
            gfxCommandClearTexture(gfx, tex_.G_normal);
            GenerateDrawIndirect(buf_.active_gaussian_count);
            gfxCommandBindKernel(gfx, kernel_.DrawActiveGaussians);
            gfxCommandBindColorTarget(gfx, 0, tex_.G_albedo_alpha);
            gfxCommandBindColorTarget(gfx, 1, tex_.G_material);
            gfxCommandBindColorTarget(gfx, 2, tex_.G_depth);
            if (!options_.reconstruct_normals) {
                gfxCommandBindColorTarget(gfx, 3, tex_.G_normal);
            }
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandMultiDrawIndirect(gfx, buf_.draw_indirect_command, 1);
#endif
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

        {
            auto section = TimedSection(*this, "FilterDepth");
            gfxCommandBindKernel(gfx, kernel_.FilterDepth);
            gfxCommandDispatch(gfx, UB.TileDimensions.x, UB.TileDimensions.y, 1);
        }

        // Generate near HZB
        {
            auto section = TimedSection(*this, "GenerateNearHZB");
            int num_mips = gfxCalculateMipCount(UB.ScreenDimensions.x, UB.ScreenDimensions.y);
            for (int i = 1; i < num_mips; i++) {
                GfxTexture in_texture = i == 1 ? tex_.G_zdepth[frame_index_ & 1] : tex_.near_HZB;
                gfxProgramSetParameter(gfx, program_, "g_InNearHZBTexture", in_texture, max(i - 2, 0));
                gfxProgramSetParameter(gfx, program_, "g_OutNearHZBTexture", tex_.near_HZB, i - 1);
                gfxCommandBindKernel(gfx, kernel_.GenerateNearHZB);
                int curr_width = divideAndRoundUp(UB.ScreenDimensions.x, (1 << i));
                int curr_height = divideAndRoundUp(UB.ScreenDimensions.y, (1 << i));
                gfxCommandDispatch(gfx,
                    divideAndRoundUp(curr_width, TILE_SIZE),
                    divideAndRoundUp(curr_height, TILE_SIZE), 1);
            }
        }

        if (options_.reconstruct_normals) {
            // Reconstruct normals from depth buffer if required
            auto section = TimedSection(*this, "ReconstructNormals");
            gfxCommandBindKernel(gfx, kernel_.ReconstructNormals);
            gfxCommandDispatch(gfx, UB.TileDimensions.x, UB.TileDimensions.y, 1);
        }

        int ray_compact_count = 0;
        auto CompactRayTraces = [&] () {
            GenerateDispatchIndirect(buf_.ray_to_trace_count[ray_compact_count & 1]);
            ray_compact_count ++;
            gfxCommandClearBuffer(gfx, buf_.ray_to_trace_count[ray_compact_count & 1]);
            gfxProgramSetParameter(gfx, program_, "g_RWCompactedRayToTraceCountBuffer", buf_.ray_to_trace_count[ray_compact_count & 1]);
            gfxProgramSetParameter(gfx, program_, "g_RWCompactedRayToTraceListBuffer", buf_.ray_to_trace_list[ray_compact_count & 1]);
            {
                auto section = TimedSection(*this, "CompactRayTraces");
                gfxCommandBindKernel(gfx, kernel_.CompactRayTraces);
#ifndef NO_INDIRECT_DISPATCH
                gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
#endif
            }
            // Swap the bound buffers
            gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceCountBuffer", buf_.ray_to_trace_count[ray_compact_count & 1]);
            gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceListBuffer", buf_.ray_to_trace_list[ray_compact_count & 1]);

        };

        // Shading begins
        {
            auto section = TimedSection(*this, "ClearFilm");
            gfxCommandClearTexture(gfx, tex_.radiance[frame_index_ & 1]);
        }

        // Direct lighting phase!

        // Spawn light samples and prepare shadow rays to be traced.
        {
            auto section = TimedSection(*this, "SampleLightRays");
            gfxCommandClearBuffer(gfx, buf_.ray_to_trace_count[0]);

            // Allocate shadow rays to be traced for DI
            gfxCommandBindKernel(gfx, kernel_.SampleLightRays);
            gfxCommandDispatch(gfx, UB.TileDimensions.x, UB.TileDimensions.y, 1);

            // Duplicate the ray count buffer, keep a record for the total number of rays before any culling
            gfxCommandCopyBuffer(gfx, buf_.ray_count, buf_.ray_to_trace_count[ray_compact_count & 1]);
        }

        // Screen space ray tracing
        if (options_.SSRT_enable) {
            auto section = TimedSection(*this, "TraceRaysInScreenSpace");
            GenerateDispatchIndirect(buf_.ray_to_trace_count[ray_compact_count & 1]);
            gfxCommandBindKernel(gfx, kernel_.TraceRaysInScreenSpace);
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
#endif
        }

        // Cull the rays that are simply completed by SSRT
        if (options_.SSRT_enable) CompactRayTraces();

        // Use hardware ray tracing (stochastic 3DGS shadow ray tracing) to deal with the rest
        if (options_.HWRT_enable) {
            auto section = TimedSection(*this, "HWRT Shadow Ray Trace");

            GenerateDispatchRaysIndirect(buf_.ray_to_trace_count[ray_compact_count & 1]);

            gfxCommandBindKernel(gfx, kernel_.Trace3DGSShadowRays);
            gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Raygen, 0, "DirectIlluminationTrace3DGSShadowRaygen");
            gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Hit, 0, "Trace3DGSShadowHitGroup");
            gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Miss, 0, "Trace3DGSShadowMiss");

#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchRaysIndirect(gfx, sbt_, buf_.dispatch_rays_indirect_command);
#endif
        }

        // Resolve the ray trace results into direct lighting
        {
            auto section = TimedSection(*this, "ResolveDirectLighting");
            GenerateDispatchIndirect(buf_.ray_count);
            gfxCommandBindKernel(gfx, kernel_.ResolveDirectLighting);
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
#endif
        }

        // Final radiance composition
        {
            auto section = TimedSection(*this, "FinalComposition");
            gfxCommandBindKernel(gfx, kernel_.FinalComposition);
            gfxCommandDispatch(gfx, UB.TileDimensions.x, UB.TileDimensions.y, 1);
        }
    }

    // Tonemapping and output
    {
        auto section = TimedSection(*this, "TonemapAndDraw");
        gfxCommandBindKernel(gfx, kernel_.TonemapAndDraw);
        gfxCommandDraw(gfx, 3, 1);
    }

    gfxDestroyBuffer(gfx, UB_range);

    frame_index_ ++;
}
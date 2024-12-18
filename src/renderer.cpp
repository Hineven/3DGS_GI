/*
 * Created: 2024/11/9
 * Author:  hineven
 * See LICENSE for licensing.
 */

#include <glm/glm.hpp>
#include <d3d12.h>
#include "gfx_imgui.h"
#include "renderer.h"

#include <ranges>

#include "device_scene.h"
#include "3dgs_shared.hlsl"

// Flag for debugging. Sometimes incorrect indirect dispatches will let my system panic.
// This flag disables all the indirect shader dispatches so i can safely check for
// shader compilation errors.
// #define NO_INDIRECT_DISPATCH
// #define NO_RAYTRACING_INDIRECT_DISPATCH

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
            "Alpha",
            "Debug"
        };

        // Check if any of the debug flags are enabled. If true, switch to debug view automatically
        bool any_debug_flag_enabled = false;
        for (auto &val: cvar_ | std::views::values) {
            bool tmp_bv = false;
            auto & ref = val;
            if (val.name.starts_with("UB.Debug_")) {
                if (val.type == CVar::BOOL) {
                    tmp_bv = val.v.i;
                    if (tmp_bv) {
                        options_.debug_mode = 6;
                        auto_switch_debug_ = true;
                        any_debug_flag_enabled = true;
                        break;
                    }
                }
            }
        }
        // Return to color mode if no debug flag is set and auto-switch is enabled.
        if (!any_debug_flag_enabled && auto_switch_debug_) {
            auto_switch_debug_ = false;
            options_.debug_mode = 0;
        }

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
        if (ImGui::SliderInt("A-trous Filter Radius", &options_.filter_radius, 1, 7)) {
            need_reload_shaders_ = true;
        }
        ImGui::Checkbox("SSRT Enable", &options_.SSRT_enable);
        ImGui::Checkbox("HWRT Enable", &options_.HWRT_enable);

        if (ImGui::CollapsingHeader("Area Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Count: %d", CB.area_light_count);
            int index_to_remove = -1;
            for (int i = 0; i < CB.area_light_count; i++) {
                ImGui::PushID(18274827 ^ i);
                ImGui::Text("Area Light %d", i);
                ImGui::SliderFloat3("Position", &CB.area_light_positions[i].x, -2, 2);
                ImGui::SliderFloat3("Facing", &CB.area_lights_facing[i].x, -1, 1);
                CB.area_lights_facing[i] = glm::normalize(CB.area_lights_facing[i]);
                ImGui::InputFloat3("Color", &CB.area_light_colors[i].x);
                ImGui::SliderFloat("Size", &CB.area_light_sizes[i], 0.05, 5);
                if (ImGui::Button("-")) {
                    index_to_remove = i;
                }
                ImGui::PopID();
            }
            if (index_to_remove != -1) {
                for (int i = index_to_remove; i < CB.area_light_count - 1; i++) {
                    CB.area_light_positions[i] = CB.area_light_positions[i + 1];
                    CB.area_lights_facing[i] = CB.area_lights_facing[i + 1];
                    CB.area_light_colors[i] = CB.area_light_colors[i + 1];
                    CB.area_light_sizes[i] = CB.area_light_sizes[i + 1];
                }
                CB.area_light_count--;
            }
            if (ImGui::Button("+") && CB.area_light_count < 10) {
                int idx = CB.area_light_count;
                CB.area_light_positions[idx] = {(idx / 10) * 2 - 1, 0, 0};
                CB.area_lights_facing[idx] = {0, 0, 1};
                CB.area_light_colors[idx] = {1, 1, 1};
                CB.area_light_sizes[idx] = 1;
                // Random initialize the triangle points on the XY plane
                glm::vec3 A, B, C;
                while (true) {
                    A = {nextFloat() * 0.1, nextFloat() * 0.1, 0};
                    B = {nextFloat() * 0.1, nextFloat() * 0.1, 0};
                    C = {nextFloat() * 0.1, nextFloat() * 0.1, 0};
                    float area = glm::length(glm::cross(B - A, C - A));
                    if (area >= 0.002) {
                        break;
                    }
                }
                if (glm::dot(glm::cross(B - A, C - A), glm::vec3(0, 0, 1)) < 0) {
                    std::swap(B, C);
                }
                float3 center = (A + B + C) / 3.f;
                A = A - center;
                B = B - center;
                C = C - center;
                CB.area_light_local_vertices[idx * 3 + 0] = A;
                CB.area_light_local_vertices[idx * 3 + 1] = B;
                CB.area_light_local_vertices[idx * 3 + 2] = C;
                CB.area_light_count ++;
            }
        }

        for (auto &val: cvar_ | std::views::values) {
            bool tmp_bv = false;
            auto & ref = val;
            switch (ref.type) {
                case CVar::BOOL:
                    tmp_bv = ref.v.i;
                    ImGui::Checkbox(ref.name.c_str(), &tmp_bv);
                    ref.v.i = tmp_bv;
                    break;
                case CVar::FLOAT:
                    if (ref.mn == ref.mx) {
                        ImGui::InputFloat(ref.name.c_str(), (float*)&ref.v, 0, 0, "%.6f");
                    } else ImGui::SliderFloat(ref.name.c_str(), (float*)&ref.v, ref.mn, ref.mx);
                    break;
                case CVar::INT:
                    if (ref.mn == ref.mx) ImGui::InputInt(ref.name.c_str(), (int*)&ref.v);
                    else ImGui::SliderInt(ref.name.c_str(), (int*)&ref.v, ref.mn, ref.mx);
                    break;
                case CVar::VEC2:
                    if (ref.mn == ref.mx) ImGui::InputFloat2(ref.name.c_str(), (float*)&ref.v);
                    else ImGui::SliderFloat2(ref.name.c_str(), (float*)&ref.v, ref.mn, ref.mx);
                    break;
                case CVar::VEC3:
                    if (ref.mn == ref.mx) ImGui::InputFloat3(ref.name.c_str(), (float*)&ref.v);
                    else ImGui::SliderFloat3(ref.name.c_str(), (float*)&ref.v, ref.mn, ref.mx);
                    break;
            }
            ImGui::SetItemTooltip("%s", ref.desc.c_str());
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

    auto registerCVar = [&](std::string name, std::string desc, auto & var_ref, auto default_value, double mn = 0, double mx = 0) {
        using raw_t = std::remove_cvref_t<decltype(var_ref)>;
        using def_t = std::remove_cvref_t<decltype(default_value)>;
        auto it = cvar_.find(&var_ref);
        if (it != cvar_.end()) {
            // use the cached value is the default behavior
        } else {
            var_ref = default_value;
            auto & cvar_ref = cvar_[(void*)&var_ref];
            cvar_ref.name = name;
            cvar_ref.desc = desc;
            cvar_ref.mn = mn;
            cvar_ref.mx = mx;
            if constexpr (std::is_same_v<def_t, bool>) {
                cvar_ref.type = CVar::BOOL;
                cvar_ref.v.i = default_value;
            } else if constexpr (std::is_integral_v<raw_t>) {
                cvar_ref.type = CVar::INT;
                cvar_ref.v.i = default_value;
            } else if constexpr (std::is_convertible_v<raw_t, float>) {
                cvar_ref.type = CVar::FLOAT;
                cvar_ref.v.f = default_value;
            } else if constexpr (std::is_same_v<raw_t, glm::vec2>) {
                cvar_ref.type = CVar::VEC2;
                cvar_ref.v.f2 = default_value;
            } else if constexpr (std::is_same_v<raw_t, glm::vec3>) {
                cvar_ref.type = CVar::VEC3;
                cvar_ref.v.f3 = default_value;
            } else {
                app_assert(false);
            }
        }
        auto & cvar_ref = cvar_[(void*)&var_ref];
        memcpy(&var_ref, &(cvar_ref.v), sizeof(raw_t));
    };
#define REGISTER_CVAR(var, desc, ...) registerCVar(#var, desc, var, __VA_ARGS__)

    UB = {};
    {
        glm::ivec2 resolution = {
                AppInternal::GetInstance().GetWindowWidth(),
                AppInternal::GetInstance().GetWindowHeight()
        };
        UB.MainCamera = camera.PackDescription(resolution.x, resolution.y, history_UB_.MainCamera);

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

        REGISTER_CVAR(UB.Debug_VisualizeLightGridCascade, "", false);

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
                radius = glm::max(radius, glm::max(glm::max(p.x, p.y), p.z));
            }
            // Expand the radius to make sure jittering won't let pixels go out of the grid.
            float mx_grid_radius = 2.f * radius / (options_.light_grid_size - 1);
            radius += mx_grid_radius;

            int fid_mapping[8] = {
                0, 7, 1, 6, 2, 5, 3, 4
            };
            int fid_mapped = fid_mapping[frame_index_% 8];
            glm::vec3 fw = glm::vec3(fid_mapped & 1, (bool)(fid_mapped & 2), (bool)(fid_mapped & 4));
            glm::vec3 jitter {nextFloat(), nextFloat(), nextFloat()};
            jitter = glm::vec3(0.5f) * fw + 0.5f * jitter;

            for (int i = 0; i < options_.light_grid_num_cascades; i++) {
                double cascade_radius = double(radius) / (1 << (options_.light_grid_num_cascades - i - 1));
                double cascade_grid_width = 2 * cascade_radius / options_.light_grid_size;
                glm::dvec3 cascade_center = glm::dvec3(camera.position) + cascade_grid_width * glm::dvec3(jitter - 0.5f);
                glm::vec3 cascade_min = glm::vec3(cascade_center - cascade_radius);
                glm::vec3 cascade_max = glm::vec3(cascade_center + cascade_radius);
                UB.LightGrid_GridCascadeMin[i] = glm::vec4(cascade_min, 0.f);
                UB.LightGrid_GridCascadeMax[i] = glm::vec4(cascade_max, 0.f);
                if (i == 0) UB.LightGrid_GridSize = cascade_grid_width;
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

        REGISTER_CVAR(UB.DI_FilterGaussianRadius, "Direct illumination spatial filter gaussian kernel multiplier.", 1.3f, 0.5f, 4.f);
        UB.DI_InvFilterGaussianRadius2 = 1.f / (UB.DI_FilterGaussianRadius * UB.DI_FilterGaussianRadius);

        REGISTER_CVAR(UB.DI_Denoiser_DepthThreshold, "Direct illumination relative depth difference threshold for depth occlusion rejection", 5e-3f);
        REGISTER_CVAR(UB.DI_NoTemporalDenoising, "Disable temporal denoising for direct illumination.", false);

        REGISTER_CVAR(UB.DI_NoSpatialDenoising, "Disable spatial denoising for direct illumination.", false);
        REGISTER_CVAR(UB.DI_Denoiser_TargetNumSamples, "The target number of samples to achieve for direct illumination denoising.", 48, 1, 100);

        REGISTER_CVAR(UB.DepthFilterRadius, "Filter radius for depth reconstruction.", 1, 0, 3);
        REGISTER_CVAR(UB.GaussianClampingScale, "Magic number for clamping the gaussian 2D eigen value to a minimum value.",
            5e-6f);

        auto im_mouse_pos = ImGui::GetMousePos();
        glm::vec2 mouse_pos = {im_mouse_pos.x, im_mouse_pos.y};
        UB.Debug_CursorPixelCoords = glm::ivec2(mouse_pos);

        REGISTER_CVAR(UB.TonemapExposure, "Exposure", 1.0f, 0.1f, 10.0f);

        UB.Card_PreferredTexelWorldSize = 0.01;

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

    history_UB_ = UB;

    // other constants
    {
        REGISTER_CVAR(CB.directional_light_dir, "", glm::vec3{0, 0, 1});
        REGISTER_CVAR(CB.directional_light_color, "", glm::vec3{1, 0.62, 0.5});
    }

    {
        LightData di = scene.GetDirectionalLight();
        di.Radiance = CB.directional_light_color;
        di.V1       = CB.directional_light_dir;
        scene.SetDirectionalLight(di);
    }
    {
        LightData ei = scene.GetSkyLight();
        // No need to do anything.
        scene.SetSkyLight(ei);
    }

    // Area lights
    {
        if (scene.GetNumLights() != CB.area_light_count + 2) {
            scene.SetNumLights(CB.area_light_count + 2);
        }
        auto toworld = [&](glm::vec3 x, glm::vec3 y, glm::vec3 z, glm::vec3 u) {
            return x * u.x + y * u.y + z * u.z;
        };
        for (int i = 0; i < CB.area_light_count; i++) {
            glm::vec3 tangent, bitangent;
            auto normal = CB.area_lights_facing[i];
            if (glm::abs(normal.x) > 0.5f) {
                tangent = glm::vec3(0, 1, 0);
            } else {
                tangent = glm::vec3(1, 0, 0);
            }
            bitangent = normalize(glm::cross(normal, tangent));
            tangent = normalize(glm::cross(bitangent, normal));
            float scale = CB.area_light_sizes[i];
            LightData L {
                CB.area_light_positions[i] + toworld(tangent, bitangent, normal, scale * CB.area_light_local_vertices[i * 3 + 0]),
                CB.area_light_positions[i] + toworld(tangent, bitangent, normal, scale * CB.area_light_local_vertices[i * 3 + 1]),
                CB.area_light_positions[i] + toworld(tangent, bitangent, normal, scale * CB.area_light_local_vertices[i * 3 + 2]),
                CB.area_light_colors[i]
            };
            scene.SetAreaLight(i, L);
        }
    }

    // Update lights
    scene.UpdateDeviceLights();

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
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceUVPositionBuffer", buf_.ray_to_trace_UV_position);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceSeedBuffer", buf_.ray_to_trace_seed);
    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceFlagsBuffer", buf_.ray_to_trace_flags);

    gfxProgramSetParameter(gfx, program_, "g_RWRayToTraceResultBuffer", buf_.ray_to_trace_result);

    gfxProgramSetParameter(gfx, program_, "g_RWDirectIlluminationRayOcclusionThresholdBuffer", buf_.direct_illumination_ray_occlusion_threshold);
    gfxProgramSetParameter(gfx, program_, "g_RWDirectIlluminationRayContributionBuffer", buf_.direct_illumination_ray_contribution);

    gfxProgramSetParameter(gfx, program_, "g_RW_GColorTexture", tex_.G_albedo_alpha);
    gfxProgramSetParameter(gfx, program_, "g_GColorTexture", tex_.G_albedo_alpha);
    gfxProgramSetParameter(gfx, program_, "g_GEmissionAlphaTexture", tex_.G_emission_alpha);
    gfxProgramSetParameter(gfx, program_, "g_RW_GEmissionAlphaTexture", tex_.G_emission_alpha);
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
    gfxProgramSetParameter(gfx, program_, "g_HistoryZDepthTexture", tex_.G_zdepth[(frame_index_ + 1) & 1]);

    gfxProgramSetParameter(gfx, program_, "g_NearHZBTexture", tex_.near_HZB);

    gfxProgramSetParameter(gfx, program_, "g_DebugTexture", tex_.debug);
    gfxProgramSetParameter(gfx, program_, "g_RW_DebugTexture", tex_.debug);

    gfxProgramSetParameter(gfx, program_, "g_RW_DirectIllumination", tex_.direct_illumination[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_DirectIllumination", tex_.direct_illumination[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_HistoryDirectIllumination", tex_.direct_illumination[(frame_index_ + 1) & 1]);
    gfxProgramSetParameter(gfx, program_, "g_RW_FilteredDirectIllumination", tex_.filtered_direct_illumination);
    gfxProgramSetParameter(gfx, program_, "g_FilteredDirectIllumination", tex_.filtered_direct_illumination);
    gfxProgramSetParameter(gfx, program_, "g_RW_Radiance", tex_.radiance[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_Radiance", tex_.radiance[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_HistoryRadiance", tex_.radiance[(frame_index_ + 1) & 1]);

    gfxProgramSetParameter(gfx, program_, "g_RasterizationDepthTexture", tex_.rasterization_depth);

    auto & samplers = AppInternal::GetInstance().GetSamplers();
    gfxProgramSetParameter(gfx, program_, "g_LinearClampSampler", samplers.linear_clamp);
    gfxProgramSetParameter(gfx, program_, "g_LinearWrapSampler", samplers.linear_wrap);
    gfxProgramSetParameter(gfx, program_, "g_PointClampSampler", samplers.point_clamp);
    gfxProgramSetParameter(gfx, program_, "g_PointWrapSampler", samplers.point_wrap);

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

    // Update mesh cards
    {
        auto roundUpPow2 = [] (int x) {
            int result = 1;
            while(result < x) {
                result *= 2;
            }
            return result;
        };
        auto check = [&] (int i, int x, int y, glm::vec2 spans) {
            for (int dx = 0; dx < spans.x; dx++) {
                for (int dy = 0; dy < spans.y; dy++) {
                    if (MC.atlas_occupancy[i][x + dx][y + dy]) {
                        return false;
                    }
                }
            }
            return true;
        };
        auto find = [&] (int i, glm::ivec2 spans) {
            for (int i = 0; i < NUM_CARD_ATLAS; i++) {
                for (int x = 0; x < CARD_ATLAS_RESOLUTION - spans.x; x++) {
                    for (int y = 0; y < CARD_ATLAS_RESOLUTION - spans.y; y++) {
                        if (check(i, x, y, spans)) {
                            return glm::ivec3(i, x, y);
                        }
                    }
                }
            }
            return glm::ivec3(-1, -1, -1);
        };
        auto fill = [&] (int i, int x, int y, glm::vec2 spans) {
            for (int dx = 0; dx < spans.x; dx++) {
                for (int dy = 0; dy < spans.y; dy++) {
                    MC.atlas_occupancy[i][x + dx][y + dy] = 1;
                }
            }
        };
        auto allocate_cards = [&] (int i, int num_cards, glm::ivec2 spans) {
            for (int j = 0; j < num_cards; j++) {
                auto pos = find(i, spans);
                if (pos.x == -1) {
                    return false;
                }
                fill(pos.x, pos.y, pos.z, spans);
                int card_header = asdasdas
            }
            return true;
        };

        // Render mesh cards if any queued (G-Buffers without lighting)
        while (MC.base_mesh_card_requests.size() > 0) {
            auto & instance_id = MC.base_mesh_card_requests.back();
            MC.base_mesh_card_requests.pop_back();
            auto aabb = scene.GetInstanceAABB(instance_id);
            auto transform = scene.GetInstanceTransform(instance_id);
            glm::vec3 scaling = {transform[0][0], transform[1][1], transform[2][2]};
            glm::vec3 world_extents = (aabb.mx - aabb.mn) * scaling;
            glm::ivec3 preferred_num_texels = glm::ivec3(world_extents / UB.Card_PreferredTexelWorldSize);
            // Find the actual mip size
            int max_num_texels = max(max(preferred_num_texels.x, preferred_num_texels.y), preferred_num_texels.z);
            static_assert((1<<MIN_CARD_RESOLUTION_L2) == MIN_CARD_RESOLUTION);
            int base_level = MIN_CARD_RESOLUTION_L2;
            while (1 << base_level < max_num_texels) {
                base_level++;
            }
            int max_resolution = min(1 << base_level, MAX_CARD_RESOLUTION);
            float factor = (float)max_resolution / max_num_texels;
            glm::ivec3 texel_counts = glm::ivec3(glm::ceil(world_extents * factor));
            for (int i = 0; i < 3; i++) {
                texel_counts[i] = roundUpPow2(glm::clamp(texel_counts[i], MIN_CARD_RESOLUTION, MAX_CARD_RESOLUTION));
            }
            // TODO allocate multiple cards for each axis adaptively
            glm::ivec3 num_cards = {1, 1, 1};
            // Find and allocate texture atlas for the mesh cards (brute force)
            glm::ivec3 spans = texel_counts / MIN_CARD_RESOLUTION;
            // yz plane
        }
    }

    {
        auto section = TimedSection(*this, "ClearTextures");
        gfxCommandClearTexture(gfx, tex_.G_albedo_alpha);
        gfxCommandClearTexture(gfx, tex_.debug);
        gfxCommandClearTexture(gfx, tex_.radiance[frame_index_ & 1]);
        gfxCommandClearTexture(gfx, tex_.direct_illumination[frame_index_ & 1]);
        gfxCommandClearTexture(gfx, tex_.filtered_direct_illumination);
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

        // Draw ordinary geometries first. The depth infomation can be further used to cull gaussians
        {
            auto section = TimedSection(*this, "DrawAreaLights");
            gfxCommandClearTexture(gfx, tex_.G_albedo_alpha);
            gfxCommandClearTexture(gfx, tex_.G_emission_alpha);
            gfxCommandClearTexture(gfx, tex_.G_material);
            gfxCommandClearTexture(gfx, tex_.G_normal);
            gfxCommandClearTexture(gfx, tex_.rasterization_depth);
            gfxCommandBindKernel(gfx, kernel_.DrawAreaLights);
            // Alpha channel is not used in this draw
            gfxCommandBindColorTarget(gfx, 0, tex_.G_albedo_alpha);
            // Alpha is drawn to this texture (0 or 1)
            gfxCommandBindColorTarget(gfx, 1, tex_.G_emission_alpha);
            gfxCommandBindColorTarget(gfx, 2, tex_.G_material);
            gfxCommandBindColorTarget(gfx, 3, tex_.G_normal);
            gfxCommandBindDepthStencilTarget(gfx, tex_.rasterization_depth);
            gfxCommandBindKernel(gfx, kernel_.DrawAreaLights);
            gfxCommandDraw(gfx, 3, CB.area_light_count);
        }

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
            gfxCommandClearTexture(gfx, tex_.G_depth);
            GenerateDrawIndirect(buf_.active_gaussian_count);
            gfxCommandBindKernel(gfx, kernel_.DrawActiveGaussians);
            gfxCommandBindColorTarget(gfx, 0, tex_.G_albedo_alpha);
            gfxCommandBindColorTarget(gfx, 1, tex_.G_material);
            gfxCommandBindColorTarget(gfx, 2, tex_.G_depth);
            gfxCommandBindDepthStencilTarget(gfx, tex_.rasterization_depth);
            if (!options_.reconstruct_normals) {
                gfxCommandBindColorTarget(gfx, 3, tex_.G_normal);
            }
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandMultiDrawIndirect(gfx, buf_.draw_indirect_command, 1);
#endif
        }

        {
            // Resolve GBuffers rendered with gaussians. Doing some normalizations for weighted sums.
            auto section = TimedSection(*this, "ResolveGBuffers");
            gfxCommandBindKernel(gfx, kernel_.ResolveGBuffers);
            auto num_threads = gfxKernelGetNumThreads(gfx, kernel_.ResolveGBuffers);
            assert(UB.ScreenDimensions.x % num_threads[0] == 0 && UB.ScreenDimensions.y % num_threads[1] == 0);
            uint num_groups_x = divideAndRoundUp((uint)UB.ScreenDimensions.x, num_threads[0]);
            uint num_groups_y = divideAndRoundUp((uint)UB.ScreenDimensions.y, num_threads[1]);
            gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
        }

        {
            // Filter the depth values rendered with gaussians.
            // Trick for reconstructing a better depth buffer for gaussians from low precision FBs and inaccurate data.
            auto section = TimedSection(*this, "FilterDepth");
            gfxCommandBindKernel(gfx, kernel_.FilterDepth);
            gfxCommandDispatch(gfx, UB.TileDimensions.x, UB.TileDimensions.y, 1);
        }

        {
            // Now, combine the G-Buffers rendered with gaussians and the ordinary G-Buffers
            auto section = TimedSection(*this, "CombineGBuffers");
            gfxCommandBindKernel(gfx, kernel_.CombineGBuffers);
            gfxCommandDispatch(gfx, UB.TileDimensions.x, UB.TileDimensions.y, 1);
        }

        // Generate near HZB
        {
            auto section = TimedSection(*this, "GenerateNearHZB");
            int num_mips = gfxCalculateMipCount(UB.ScreenDimensions.x, UB.ScreenDimensions.y);
            for (int i = 1; i < num_mips; i++) {
                GfxTexture in_texture = i == 1 ? tex_.G_zdepth[frame_index_ & 1] : tex_.near_HZB;
                gfxProgramSetParameter(gfx, program_, "g_InNearHZBTexture", in_texture, std::max(i - 2, 0));
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
            auto section = TimedSection(*this, "InitializeCounters");
            gfxCommandBindKernel(gfx, kernel_.InitializeCounters);
            gfxCommandDispatch(gfx, 1, 1, 1);
        }

        // Direct lighting phase!

        // Update light headers for injection and sampling
        {
            auto section = TimedSection(*this, "UpdateLightHeaders");
            gfxCommandBindKernel(gfx, kernel_.UpdateLightHeaders);
            auto threads = gfxKernelGetNumThreads(gfx, kernel_.UpdateLightHeaders);
            int num_lights = scene.GetNumLights();
            gfxCommandDispatch(gfx, divideAndRoundUp(num_lights, (int)threads[0]), 1, 1);
        }

        // Inject lights into the light grid
        {
            auto section = TimedSection(*this, "InjectLights");
            gfxCommandBindKernel(gfx, kernel_.InjectLights);
            // int num_lights = scene.GetNumLights();
            int num_grids = UB.LightGrid_GridResolution3 * UB.LightGrid_NumGridCascades;
            auto threads = gfxKernelGetNumThreads(gfx, kernel_.InjectLights);
            gfxCommandDispatch(gfx, divideAndRoundUp(num_grids, (int)threads[0]), 1, 1);
        }

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

            gfxCommandBindKernel(gfx, kernel_.DirectIlluminationTrace3DGSShadowRays);
            gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Raygen, 0, "DirectIlluminationTrace3DGSShadowRaygen");
            gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Hit, 0, "Trace3DGSShadowHitGroup");
            gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Miss, 0, "Trace3DGSShadowMiss");

#if !(defined(NO_INDIRECT_DISPATCH) || defined(NO_RAYTRACING_INDIRECT_DISPATCH))
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

        // Filter direct illumination
        if (!UB.DI_NoSpatialDenoising) {
            {
                auto section = TimedSection(*this, "SpatialFilterDirectIllumination Pass #0");
                gfxCommandBindKernel(gfx, kernel_.SpatialFilterDirectIllumination[0]);
                gfxCommandDispatch(gfx, UB.TileDimensions.x, UB.TileDimensions.y, 1);
            }

            {
                auto section = TimedSection(*this, "SpatialFilterDirectIllumination Pass #1");
                gfxCommandBindKernel(gfx, kernel_.SpatialFilterDirectIllumination[1]);
                gfxCommandDispatch(gfx, UB.TileDimensions.x, UB.TileDimensions.y, 1);
            }
        }

        {
            auto section = TimedSection(*this, "TemporalFilterDirectIllumination");
            gfxCommandBindKernel(gfx, kernel_.TemporalFilterDirectIllumination);
            gfxCommandDispatch(gfx, UB.TileDimensions.x, UB.TileDimensions.y, 1);

            gfxCommandCopyTexture(gfx, tex_.direct_illumination[frame_index_ & 1], tex_.filtered_direct_illumination);
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
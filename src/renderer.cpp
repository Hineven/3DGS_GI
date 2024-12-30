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
#include "glm/gtx/matrix_decompose.hpp"

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

void Renderer::UploadBufferStaged(GfxBuffer buf, const void *data, size_t size) {
    if (!size) return ;
    auto gfx = AppInternal::GetInstance().GetGfx();
    int id = frame_index_ % kGfxConstant_BackBufferCount;
    if (frame_index_ != staging_buffer_frame_index_[id]) {
        staging_buffer_frame_index_[id] = frame_index_;
        staging_buffer_frame_offset_[id] = 0;
    }
    if (staging_buffer_[id].getSize() < staging_buffer_frame_offset_[id] + size) {
        auto new_size = std::max(staging_buffer_frame_offset_[id] + size, staging_buffer_[id].getSize() * 2);
        if (staging_buffer_[id]) gfxDestroyBuffer(gfx, staging_buffer_[id]);
        staging_buffer_[id] = gfxCreateBuffer(gfx, new_size, nullptr, kGfxCpuAccess_Write);
        staging_buffer_[id].setName("RendererStagingBuffer");
    }
    memcpy((char*)gfxBufferGetData(gfx, staging_buffer_[id]) + staging_buffer_frame_offset_[id], data, size);
    gfxCommandCopyBuffer(gfx, buf, 0, staging_buffer_[id], staging_buffer_frame_offset_[id], size);
    staging_buffer_frame_offset_[id] += size;
}

void Renderer::ResetStagingBuffers() {
    auto gfx = AppInternal::GetInstance().GetGfx();
    for (auto & buf : staging_buffer_) {
        if (buf) gfxDestroyBuffer(gfx, buf);
        buf = {};
    }
    for (int i = 0; i < kGfxConstant_BackBufferCount; i++) {
        staging_buffer_frame_index_[i] = -1;
        staging_buffer_frame_offset_[i] = 0;
    }
}

GfxBuffer Renderer::AllocateUBForCurrentFrame(size_t size) {
    size = roundUp((uint32_t)size, 256u);
    int id = frame_index_ % kGfxConstant_BackBufferCount;
    if (UB_pool_allocation_frames_[id] != frame_index_) {
        UB_pool_allocation_frames_[id] = frame_index_;
        UB_pool_allocation_sizes_[id] = 0;
    }
    if (cfg_.uniform_buffer_size - UB_pool_allocation_offset_ < size) {
        UB_pool_allocation_sizes_[id] += cfg_.uniform_buffer_size - UB_pool_allocation_offset_;
        UB_pool_allocation_offset_ = 0;
    }
    int sum_sizes = 0;
    for (auto e : UB_pool_allocation_sizes_) {
        sum_sizes += e;
    }
    if (sum_sizes + size > cfg_.uniform_buffer_size) {
        app_warning("Uniform buffer pool is exhausted");
        return GfxBuffer();
    }
    auto gfx = AppInternal::GetInstance().GetGfx();
    GfxBuffer buf = gfxCreateBufferRange(gfx, buf_.UB_pool, UB_pool_allocation_offset_, size);
    UB_pool_allocation_offset_ += size;
    UB_pool_allocation_sizes_[id] += size;
    return buf;
}

void Renderer::ResetUniformBufferPool() {
    for (int i = 0; i < kGfxConstant_BackBufferCount; i++) {
        UB_pool_allocation_frames_[i] = -1;
        UB_pool_allocation_sizes_[i] = 0;
    }
    UB_pool_allocation_offset_ = 0;
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
        if (ImGui::Button("Rest HashGrids")) {
            should_reset_hash_grids_ = true;
        }
        auto & scene = AppInternal::GetInstance().GetScene();
        auto & device_scene = scene.GetDeviceScene();
        auto gfx = AppInternal::GetInstance().GetGfx();
        is_instance_active_.resize(scene.GetNumInstances());
        if (ImGui::CollapsingHeader("Instances", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (int i = 0; i < is_instance_active_.size(); i++) {
                std::string id = "Instance " + std::to_string(i);
                ImGui::PushID(id.c_str());
                bool bv = is_instance_active_[i];
                ImGui::Checkbox(id.c_str(), &bv);
                if (is_instance_active_[i] != bv) {
                    gfxSetRaytracingPrimitiveActive(gfx, device_scene.rt_primitives_[i], bv);
                    should_rebuild_TLAS_ = true;
                }
                is_instance_active_[i] = bv;
                InstanceTransform model = scene.GetInstanceTransform(i);
                bool changed = false;
                changed |= ImGui::InputFloat3("Position", &model.position.x);
                model.rotation = glm::degrees(model.rotation);
                changed |= ImGui::SliderFloat3("Rotation", &model.rotation.x, -180, 180);
                model.rotation = glm::radians(model.rotation);
                changed |= ImGui::InputFloat3("Scale", &model.scale.x);
                if (changed) {
                    scene.SetInstanceTransform(i, model);
                    should_update_transforms_ = true;
                }
                ImGui::PopID();
            }
        }
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

        if (ImGui::CollapsingHeader("Mesh Cards", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Button("Redraw All Meshcards")) {
                RequestRedrawAllMeshCardsForAllInstances();
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
        UB.PreviousMainCamera = history_UB_.MainCamera;

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

        UB.InvScreenDimensions = glm::vec2(glm::dvec2(1.f) / glm::dvec2(UB.ScreenDimensions));
        UB.InvTileDimensions = glm::vec2(glm::dvec2(1.f) / glm::dvec2(UB.TileDimensions));

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

        REGISTER_CVAR(UB.DI_Denoiser_DepthThreshold, "Direct illumination relative depth difference threshold for depth occlusion rejection", 1e-3f);
        REGISTER_CVAR(UB.DI_NoTemporalDenoising, "Disable temporal denoising for direct illumination.", false);

        REGISTER_CVAR(UB.DI_NoSpatialDenoising, "Disable spatial denoising for direct illumination.", false);
        REGISTER_CVAR(UB.DI_Denoiser_TargetNumSamples, "The target number of samples to achieve for direct illumination denoising.", 16, 1, 100);

        REGISTER_CVAR(UB.II_NoTemporalDenoising, "Disable temporal denoising for indirect illumination.", false);
        REGISTER_CVAR(UB.II_Denoiser_TargetNumSamples, "The target number of samples to achieve for indirect illumination denoising.", 12, 1, 100);
        REGISTER_CVAR(UB.II_SecondaryVertexNormalOffset, "Offset along the normal of the secondary vertex when spawning shadow rays for direct illumination.", 2e-2f, 0.f, 0.5f);
        REGISTER_CVAR(UB.SSRC_ProbeFiltering, "Enable probe filtering for SSRC.", true);

        REGISTER_CVAR(UB.SSRC_NoImportanceSampling, "Disable importance sampling for SSRC.", false);
        UB.SSRC_NumUniformScreenProbes = UB.TileDimensions.x * UB.TileDimensions.y;
        REGISTER_CVAR(UB.SSRC_BaseUpdateRayWaves, "Number of probe update rays allocated for each probe, in waves.", 1, 1, SSRC_MAX_NUM_UPDATE_RAY_PER_PROBE / cfg_.wave_lane_count);
        REGISTER_CVAR(UB.SSRC_ResetCache, "Reset SSRC probes at the begging of each frame.", false);

        UB.SSRC_MaxNumAdaptiveProbes = options_.SSRC_max_num_probes - UB.SSRC_NumUniformScreenProbes;
        REGISTER_CVAR(UB.SSRC_NoAdaptiveProbes, "Do not allocate adaptive probes for SSRC.", false);
        REGISTER_CVAR(CB.SSRC_freeze_tile_jitter, "Freeze the tile jitter for SSRC.", false);
        REGISTER_CVAR(CB.SSRC_tile_jitter, "Tile jitter for SSRC (frozen).", 0, 0, 7);
        if (CB.SSRC_freeze_tile_jitter) {
            UB.SSRC_TileJitterFrameSeed = CB.SSRC_tile_jitter;
        } else UB.SSRC_TileJitterFrameSeed = frame_index_ % 8;
        UB.SSRC_PreviousTileJitterFrameSeed = history_UB_.SSRC_TileJitterFrameSeed;

        UB.TAAJitterUV = glm::vec2(0);
        REGISTER_CVAR(UB.HashGrids_MaxNumSamples, "Maximum number of samples kept in each hash grid cell.", 64, 0, 128);
        UB.HashGrids_MaxNumTiles = options_.HashGrids_max_num_tiles;
        UB.HashGrids_Center = camera.position;
        REGISTER_CVAR(CB.HashGrids_cascade_radius, "Radius of the base cascade in the hash grid", 2.f, 1.f, 10.f);
        UB.HashGrids_InvCascadeRadius = 1.f / CB.HashGrids_cascade_radius;

        REGISTER_CVAR(UB.HashGrids_CellSize, "Size of base level cell in hash grids.", 0.04f, 0.01f, 0.2f);
        UB.HashGrids_NumBuckets = options_.HashGrids_max_num_buckets;
        UB.HashGrids_NumInterleavedEntriesPerBucket = options_.HashGrids_num_slots_per_bucket;
        REGISTER_CVAR(UB.HashGrids_TargetSampleCount, "Target number of samples to achieve for hash grid sampling.", 16, 1, 128);
        REGISTER_CVAR(UB.HashGrids_TileLifespan, "Tile lifespan in hash grids (max number of frames unvisited).", 30, 1, 60);
        REGISTER_CVAR(UB.HashGrids_MaxNumEntriesSearchedPerBucket, "Maximum number of entries to search when query hash table for a tile.", 8, 1, HASHGRIDS_MAX_NUM_ENTRIES_SEARCHED_PER_BUCKET);
        UB.PreviousTAAJitterUV = history_UB_.TAAJitterUV;


        REGISTER_CVAR(UB.DepthFilterRadius, "Filter radius for depth reconstruction.", 1, 0, 3);
        REGISTER_CVAR(UB.GaussianClampingScale, "Magic number for clamping the gaussian 2D eigen value to a minimum value.",
            1e-3f);
        UB.Card_PreferredTexelWorldSize = 0.01;
        REGISTER_CVAR(UB.Debug_CardSetToVisualize, "Index of the card set to visualize.", 0);

        REGISTER_CVAR(UB.Card_SampleZDepthVisibilityBias, "Visibility bias scale for card sampling.", 0.01f, 0.f, 0.1f);
        REGISTER_CVAR(UB.Card_MinCardViewDirectionWeightToSample, "Minimum view direction weight to sample on an card axis.", 0.f, 0.f, 1.f);
        REGISTER_CVAR(UB.Card_GaussianClampingScale, "Magic number for clamping the gaussian 2D eigen value to a minimum value "
                                                     "(upon rendering meshcards).",
            1e-3f);
        REGISTER_CVAR(UB.TonemapExposure, "Exposure", 1.0f, 0.1f, 10.0f);

        REGISTER_CVAR(UB.NoDirectIllumination, "Disable direct illumination in final composition.", false);
        REGISTER_CVAR(UB.NoIndirectIllumination, "Disable indirect illumination in final composition.", false);

        REGISTER_CVAR(UB.LightingSkyRadianceLOD, "LOD when sampling lighting from the sky.", 0, 0, 5);
        REGISTER_CVAR(UB.Debug_VisualizeLightGridCascade, "", false);
        auto im_mouse_pos = ImGui::GetMousePos();
        glm::vec2 mouse_pos = {im_mouse_pos.x, im_mouse_pos.y};
        UB.Debug_CursorPixelCoords = glm::ivec2(mouse_pos);
        REGISTER_CVAR(UB.Debug_VisualizeMeshCardScene, "Shade the scene using meshcards queries.", false);

        REGISTER_CVAR(UB.Debug_VisualizeMeshCardAtlas, "Display the atlas directly.", false);
        REGISTER_CVAR(UB.Debug_VisualizeMeshCardAtlasLayer, "Layer of the atlas to visualize.", 0);
        REGISTER_CVAR(UB.Debug_VisualizeMeshCardAtlasChannel, "Channel", 0, 0, 3);
        REGISTER_CVAR(UB.Debug_VisualizeMeshCardAtlasScale, "Scale", 0.5, 0.01, 1);

        REGISTER_CVAR(UB.Debug_SSRC_VisualizeProbeUpdateRays, "Visualize probe update rays for SSRC probes.", false);
        REGISTER_CVAR(UB.Debug_SSRC_VisualizeProbes, "Visualize SSRC probe allocation.", false);

        UB.SSRC_ProbeUpdateRaySampleSeed = UB.FrameIndex;
        REGISTER_CVAR(UB.SSRC_FixedProbeUpdateRaySampleSeed, "Fix the probe update ray sample seed.", false);
        if (UB.SSRC_FixedProbeUpdateRaySampleSeed) {
            UB.SSRC_ProbeUpdateRaySampleSeed = 0;
        }

        REGISTER_CVAR(UB.Debug_VisualizeMeshCardAtlasOffset, "Offset", glm::vec2(0, 0), 0, CARD_ATLAS_RESOLUTION);

        gfxSbtGetGpuVirtualAddressRangeAndStride(gfx, sbt_,
            (D3D12_GPU_VIRTUAL_ADDRESS_RANGE *)&UB.RT_RayGenerationShaderRecord,
            (D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE *)&UB.RT_MissShaderTable,
            (D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE *)&UB.RT_HitGroupTable,
            (D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE *)&UB.RT_CallableShaderTable);

    }
    GfxBuffer UB_range = AllocateUBForCurrentFrame<UniformBlock>();
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
        if (scene.GetNumLights() != CB.scene_area_light_count + CB.area_light_count + 2) {
            scene.SetNumLights(CB.scene_area_light_count + CB.area_light_count + 2);
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
            scene.SetAreaLight(CB.scene_area_light_count + i, L);
        }
    }

    // Update lights
    scene.UpdateDeviceLights();

    if (should_update_transforms_) {
        scene.UpdateDeviceTransforms();
        should_rebuild_TLAS_ = true;
        should_update_transforms_ = false;
    }

    auto & device_scene = scene.GetDeviceScene();
    device_scene.Bind(program_);

    blue_noise_sampler_.InstallParameters(program_);

    // Light grid
    gfxProgramSetBuffer(gfx, program_, "g_LightGrid_GridLightListAllocator", buf_.LightGrid_grid_light_list_allocator);
    gfxProgramSetBuffer(gfx, program_, "g_LightGrid_GridLightCountBuffer", buf_.LightGrid_grid_light_count);
    gfxProgramSetBuffer(gfx, program_, "g_LightGrid_GridLightListOffsetBuffer", buf_.LightGrid_grid_light_list_offset);
    gfxProgramSetBuffer(gfx, program_, "g_LightGrid_GridLightListBuffer", buf_.LightGrid_grid_light_list);

    // Hash grids
    gfxProgramSetBuffer(gfx, program_, "g_HashGrids_FreeTileCountBuffer", buf_.HashGrids_free_tile_count);
    gfxProgramSetBuffer(gfx, program_, "g_HashGrids_FreeTileListBuffer", buf_.HashGrids_free_tile_list);
    gfxProgramSetBuffer(gfx, program_, "g_HashGrids_BucketHashBuffer", buf_.HashGrids_bucket_hash);
    gfxProgramSetBuffer(gfx, program_, "g_HashGrids_BucketTileIndexBuffer", buf_.HashGrids_bucket_tile_index);
    gfxProgramSetBuffer(gfx, program_, "g_HashGrids_TileTimestampBuffer", buf_.HashGrids_tile_timestamp);
    gfxProgramSetBuffer(gfx, program_, "g_HashGrids_TileBucketHashBuffer", buf_.HashGrids_tile_bucket_hash);
    gfxProgramSetBuffer(gfx, program_, "g_HashGrids_CellValueBuffer", buf_.HashGrids_cell_value);
    gfxProgramSetBuffer(gfx, program_, "g_HashGrids_UpdateCellValueXBuffer", buf_.HashGrids_update_cell_value_X);
    gfxProgramSetBuffer(gfx, program_, "g_HashGrids_UpdateTileCountBuffer", buf_.HashGrids_update_tile_count);
    gfxProgramSetBuffer(gfx, program_, "g_HashGrids_UpdateTileListBuffer", buf_.HashGrids_update_tile_list);
    gfxProgramSetBuffer(gfx, program_, "g_HashGrids_ActiveTileCountBeforeAllocationBuffer", buf_.HashGrids_active_tile_count_before_allocation);
    gfxProgramSetBuffer(gfx, program_, "g_HashGrids_ActiveTileCountBuffer", buf_.HashGrids_active_tile_count[frame_index_ & 1]);
    gfxProgramSetBuffer(gfx, program_, "g_HashGrids_ActiveTileListBuffer", buf_.HashGrids_active_tile_list[frame_index_ & 1]);
    gfxProgramSetBuffer(gfx, program_, "g_HashGrids_HistoryActiveTileCountBuffer", buf_.HashGrids_active_tile_count[!(frame_index_ & 1)]);
    gfxProgramSetBuffer(gfx, program_, "g_HashGrids_HistoryActiveTileListBuffer", buf_.HashGrids_active_tile_list[!(frame_index_ & 1)]);

    // SSRC
    gfxProgramSetParameter(gfx, program_, "g_RWProbeDispatchCommandBuffer", buf_.probe_dispatch_command);
    gfxProgramSetParameter(gfx, program_, "g_RWProbePerLaneDispatchCommandBuffer", buf_.probe_per_lane_dispatch_command);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeUpdateRayReduceCountBuffer", buf_.probe_update_ray_reduce_count);

    gfxProgramSetParameter(gfx, program_, "g_RWProbeScreenCoordsTexture", tex_.probe_screen_coords[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeLinearDepthTexture", tex_.probe_linear_depth[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeWorldPositionTexture", tex_.probe_world_position[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeNormalTexture", tex_.probe_normal[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_RWPreviousProbeScreenCoordsTexture", tex_.probe_screen_coords[!(frame_index_ & 1)]);
    gfxProgramSetParameter(gfx, program_, "g_RWPreviousProbeLinearDepthTexture", tex_.probe_linear_depth[!(frame_index_ & 1)]);
    gfxProgramSetParameter(gfx, program_, "g_RWPreviousProbeWorldPositionTexture", tex_.probe_world_position[!(frame_index_ & 1)]);
    gfxProgramSetParameter(gfx, program_, "g_RWPreviousProbeNormalTexture", tex_.probe_normal[!(frame_index_ & 1)]);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeColorTexture", tex_.probe_color[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_RWPreviousProbeColorTexture", tex_.probe_color[!(frame_index_ & 1)]);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeSampleColorTexture", tex_.probe_sample_color);
    gfxProgramSetParameter(gfx, program_, "g_ProbeSampleColorTexture", tex_.probe_sample_color);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeSHCoefficientsRTexture", tex_.probe_SH_coefficients_R);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeSHCoefficientsGTexture", tex_.probe_SH_coefficients_G);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeSHCoefficientsBTexture", tex_.probe_SH_coefficients_B);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeIrradianceTexture", tex_.probe_irradiance);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeHistoryTrustTexture", tex_.probe_history_trust);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeUpdateRayCountsBuffer", buf_.probe_update_ray_counts);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeUpdateRayOffsetsBuffer", buf_.probe_update_ray_offsets);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeAllUpdateRayCountBuffer", buf_.probe_all_update_ray_count);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeUpdateRayProbeBuffer", buf_.probe_update_ray_probe);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeUpdateRayDirectionBuffer", buf_.probe_update_ray_direction);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeUpdateRayResultBuffer", buf_.probe_update_ray_result);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeUpdateRayDepthBuffer", buf_.probe_update_ray_depth);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeUpdateRayHitShadeCountBuffer", buf_.probe_update_ray_hit_shade_count);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeUpdateRayHitShadeListBuffer", buf_.probe_update_ray_hit_shade_list);
    gfxProgramSetParameter(gfx, program_, "g_RWProbeUpdateRayResolveHashCellIndexBuffer", buf_.probe_update_ray_resolve_hash_cell_index);
    gfxProgramSetParameter(gfx, program_, "g_RWTileAdaptiveProbeCountTexture", tex_.tile_adaptive_probe_count[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_RWPreviousTileAdaptiveProbeCountTexture", tex_.tile_adaptive_probe_count[!(frame_index_ & 1)]);
    gfxProgramSetParameter(gfx, program_, "g_RWTileAdaptiveProbeIndexTexture", tex_.tile_adaptive_probe_index[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_RWPreviousTileAdaptiveProbeIndexTexture", tex_.tile_adaptive_probe_index[!(frame_index_ & 1)]);
    gfxProgramSetParameter(gfx, program_, "g_RWAdaptiveProbeCountBuffer", buf_.adaptive_probe_count);

    // Common
    gfxProgramSetParameter(gfx, program_, "g_RWDispatchIndirectCommandBuffer", buf_.dispatch_indirect_command);
    gfxProgramSetParameter(gfx, program_, "g_RWDispatchRaysIndirectCommandBuffer", buf_.dispatch_rays_indirect_command);
    gfxProgramSetParameter(gfx, program_, "g_RWDrawIndirectCommandBuffer", buf_.draw_indirect_command);

    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianCountBuffer", buf_.active_gaussian_count);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianListBuffer", buf_.active_gaussian_list);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianIndirectBuffer", buf_.active_gaussian_indirect);
    gfxProgramSetParameter(gfx, program_, "g_RWActiveGaussianIndirectSrcBuffer", buf_.active_gaussian_indirect_src);
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
    gfxProgramSetParameter(gfx, program_, "g_RWDirectIlluminationRayProbeUpdateRayIndexBuffer", buf_.direct_illumination_ray_probe_update_ray_index);

    gfxProgramSetParameter(gfx, program_, "g_RW_GColorTexture", tex_.G_albedo_alpha);
    gfxProgramSetParameter(gfx, program_, "g_GColorTexture", tex_.G_albedo_alpha);
    gfxProgramSetParameter(gfx, program_, "g_GEmissionAlphaTexture", tex_.G_emission_alpha);
    gfxProgramSetParameter(gfx, program_, "g_RW_GEmissionAlphaTexture", tex_.G_emission_alpha);
    gfxProgramSetParameter(gfx, program_, "g_RW_GDepthTexture", tex_.G_depth);
    gfxProgramSetParameter(gfx, program_, "g_GDepthTexture", tex_.G_depth);
    gfxProgramSetParameter(gfx, program_, "g_RW_GMaterialTexture", tex_.G_material);
    gfxProgramSetParameter(gfx, program_, "g_GMaterialTexture", tex_.G_material);
    // Not rasterized, but derived from depth buffer (overdraw is too severe for 3dgs)
    gfxProgramSetParameter(gfx, program_, "g_RW_GNormalTexture", tex_.G_normal[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_GNormalTexture", tex_.G_normal[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_HistoryNormalTexture", tex_.G_normal[!(frame_index_ & 1)]);
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

    gfxProgramSetParameter(gfx, program_, "g_RW_IndirectIllumination", tex_.indirect_illumination[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_IndirectIllumination", tex_.indirect_illumination[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_HistoryIndirectIllumination", tex_.indirect_illumination[(frame_index_ + 1) & 1]);
    gfxProgramSetParameter(gfx, program_, "g_RW_FilteredIndirectIllumination", tex_.filtered_indirect_illumination);
    gfxProgramSetParameter(gfx, program_, "g_FilteredIndirectIllumination", tex_.filtered_indirect_illumination);

    gfxProgramSetParameter(gfx, program_, "g_RW_Radiance", tex_.radiance[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_Radiance", tex_.radiance[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_HistoryRadiance", tex_.radiance[(frame_index_ + 1) & 1]);
    gfxProgramSetParameter(gfx, program_, "g_RW_HistoryRadianceWithoutEmission", tex_.history_radiance_without_emission);
    gfxProgramSetParameter(gfx, program_, "g_HistoryRadianceWithoutEmission", tex_.history_radiance_without_emission);

    // Cards
    gfxProgramSetParameter(gfx, program_, "g_CardSets", buf_.card_sets);
    gfxProgramSetParameter(gfx, program_, "g_Cards", buf_.cards);
    gfxProgramSetParameter(gfx, program_, "g_RWCardAtlas_ColorTexture", tex_.card_atlas_color);
    gfxProgramSetParameter(gfx, program_, "g_CardAtlas_ColorTexture", tex_.card_atlas_color);
    gfxProgramSetParameter(gfx, program_, "g_RWCardAtlas_AlphaTexture", tex_.card_atlas_alpha);
    gfxProgramSetParameter(gfx, program_, "g_CardAtlas_AlphaTexture", tex_.card_atlas_alpha);
    gfxProgramSetParameter(gfx, program_, "g_RWCardAtlas_NormalTexture", tex_.card_atlas_normal);
    gfxProgramSetParameter(gfx, program_, "g_CardAtlas_NormalTexture", tex_.card_atlas_normal);
    gfxProgramSetParameter(gfx, program_, "g_RWCardAtlas_LinearDepthTexture", tex_.card_atlas_linear_depth);
    gfxProgramSetParameter(gfx, program_, "g_CardAtlas_LinearDepthTexture", tex_.card_atlas_linear_depth);
    gfxProgramSetParameter(gfx, program_, "g_RWCardAtlas_DirectIlluminationTexture", tex_.card_atlas_direct_illumination);
    gfxProgramSetParameter(gfx, program_, "g_CardAtlas_DirectIlluminationTexture", tex_.card_atlas_direct_illumination);
    gfxProgramSetParameter(gfx, program_, "g_RWCardAtlas_IndirectIlluminationTexture", tex_.card_atlas_indirect_illumination);
    gfxProgramSetParameter(gfx, program_, "g_CardAtlas_IndirectIlluminationTexture", tex_.card_atlas_indirect_illumination);
    gfxProgramSetParameter(gfx, program_, "g_RWCardAtlas_LightingTexture", tex_.card_atlas_lighting);
    gfxProgramSetParameter(gfx, program_, "g_CardAtlas_LightingTexture", tex_.card_atlas_lighting);

    gfxProgramSetParameter(gfx, program_, "g_CardWorkspace_ColorAlphaTexture", tex_.card_workspace_color_alpha);
    gfxProgramSetParameter(gfx, program_, "g_RWCardWorkspace_ColorAlphaTexture", tex_.card_workspace_color_alpha);
    gfxProgramSetParameter(gfx, program_, "g_CardWorkspace_NormalTexture", tex_.card_workspace_normal);
    gfxProgramSetParameter(gfx, program_, "g_RWCardWorkspace_NormalTexture", tex_.card_workspace_normal);
    gfxProgramSetParameter(gfx, program_, "g_CardWorkspace_LinearDepthTexture", tex_.card_workspace_linear_depth);
    gfxProgramSetParameter(gfx, program_, "g_RWCardWorkspace_LinearDepthTexture", tex_.card_workspace_linear_depth);

    gfxProgramSetParameter(gfx, program_, "g_CardWorkspace_DirectIlluminationTexture", tex_.card_workspace_direct_illumination[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_RWCardWorkspace_DirectIlluminationTexture", tex_.card_workspace_direct_illumination[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_CardWorkspace_HistoryDirectIlluminationTexture", tex_.card_workspace_direct_illumination[(frame_index_ + 1) & 1]);
    gfxProgramSetParameter(gfx, program_, "g_CardWorkspace_IndirectIlluminationTexture", tex_.card_workspace_indirect_illumination[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_RWCardWorkspace_IndirectIlluminationTexture", tex_.card_workspace_indirect_illumination[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_CardWorkspace_HistoryIndirectIlluminationTexture", tex_.card_workspace_indirect_illumination[(frame_index_ + 1) & 1]);
    gfxProgramSetParameter(gfx, program_, "g_CardWorkspace_LightingTexture", tex_.card_workspace_lighting[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_RWCardWorkspace_LightingTexture", tex_.card_workspace_lighting[frame_index_ & 1]);
    gfxProgramSetParameter(gfx, program_, "g_CardWorkspace_HistoryLightingTexture", tex_.card_workspace_lighting[(frame_index_ + 1) & 1]);

    gfxProgramSetParameter(gfx, program_, "g_RasterizationDepthTexture", tex_.rasterization_depth);

    gfxProgramSetParameter(gfx, program_, "g_ShadingLUTTexture", tex_.shading_LUT);

    auto & samplers = AppInternal::GetInstance().GetSamplers();
    gfxProgramSetParameter(gfx, program_, "g_LinearClampSampler", samplers.linear_clamp);
    gfxProgramSetParameter(gfx, program_, "g_LinearWrapSampler", samplers.linear_wrap);
    gfxProgramSetParameter(gfx, program_, "g_PointClampSampler", samplers.point_clamp);
    gfxProgramSetParameter(gfx, program_, "g_PointWrapSampler", samplers.point_wrap);

    gfxProgramSetBuffer(gfx, program_, "UB", UB_range);

    // Debugging...
#ifndef NDEBUG
    gfxProgramSetParameter(gfx, program_, "g_Debug_SSRC_ProbeIndexBuffer", buf_.Debug_SSRC_probe_index);
    gfxProgramSetParameter(gfx, program_, "g_Debug_DirectIlluminationPixelRayIndexBuffer", buf_.Debug_direct_illumination_pixel_ray_index);
    gfxProgramSetParameter(gfx, program_, "g_Debug_VisualizeRayCountBuffer", buf_.Debug_visualize_ray_count);
    gfxProgramSetParameter(gfx, program_, "g_Debug_VisualizeRayVertexBuffer", buf_.Debug_visualize_ray_vertex);
    gfxProgramSetParameter(gfx, program_, "g_Debug_VisualizeRayColorBuffer", buf_.Debug_visualize_ray_color);
    gfxProgramSetParameter(gfx, program_, "g_Debug_VisualizeRayRayIndexBuffer", buf_.Debug_visualize_ray_ray_index);
#endif

    // Rendering begins

    // Build the acceleration structure if required
    if(should_build_acceleration_structure_) {
        // Do not perform any primitive level updates unless they are no longer used.
        gfxFinish(gfx);
        std::cout << "Building acceleration structure" << std::endl;
        for(auto primitive : device_scene.rt_primitives_) {
            gfxDestroyRaytracingPrimitive(gfx, primitive);
        }
        device_scene.rt_primitives_.clear();
        if (device_scene.acceleration_structure_) {
            gfxDestroyAccelerationStructure(gfx, device_scene.acceleration_structure_);
        }
        device_scene.acceleration_structure_ = gfxCreateAccelerationStructure(gfx);
        GfxBuffer vertex_buffer = gfxCreateBuffer<glm::vec3>(gfx, 12 * scene.GetNumGaussians());
        GfxBuffer index_buffer = gfxCreateBuffer<int>(gfx, 60 * scene.GetNumGaussians());

        gfxProgramSetParameter(gfx, program_, "g_RW_RTVertexBuffer", vertex_buffer);
        gfxProgramSetParameter(gfx, program_, "g_RW_RTIndexBuffer", index_buffer);

        auto timed_section = TimedSection(*this, "GenerateRTMesh");
        gfxCommandBindKernel(gfx, kernel_.GenerateRTMesh);
        auto num_threads = gfxKernelGetNumThreads(gfx, kernel_.GenerateRTMesh);
        for (int i = 0; i < scene.GetNumInstances(); i++) {
            auto inst = scene.GetInstance(i);
            if (inst.type != InstanceType::eGaussians) {
                continue;
            }
            int inst_gaussians = scene.GetInstanceNumGaussians(i);
            gfxProgramSetParameter(gfx, program_, "g_GenerateRTMesh_InstanceIndex", i);
            gfxCommandDispatch(gfx, divideAndRoundUp(inst_gaussians, (int)num_threads[0]), 1, 1);
        }
        // Wait for the command to finish
        gfxFinish(gfx);

        for(auto primitive : device_scene.rt_primitives_) {
            gfxDestroyRaytracingPrimitive(gfx, primitive);
        }
        device_scene.rt_primitives_.resize(scene.GetNumInstances());

        for(int i = 0; i < scene.GetNumInstances(); i++) {
            auto inst = scene.GetInstance(i);
            if (inst.type == InstanceType::eGaussians) {
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
            } else {
                device_scene.rt_primitives_[i] = gfxCreateRaytracingPrimitive(gfx, device_scene.acceleration_structure_);
                auto index_range = gfxCreateBufferRange(
                        gfx, device_scene.gsi_indices_, scene.gsi_mesh_index_offsets_[i] * sizeof(int),
                        scene.gsi_mesh_num_indices_[i] * sizeof(int));
                auto vertex_range = gfxCreateBufferRange(
                        gfx, device_scene.gsi_vertices_, scene.gsi_gs_index_offsets_[i] * sizeof(glm::vec3),
                        scene.gsi_gs_counts_[i] * sizeof(Vertex));
                vertex_range.setStride(sizeof(Vertex));
                gfxRaytracingPrimitiveBuild(
                        gfx, device_scene.rt_primitives_[i], index_range, vertex_range, sizeof(Vertex)
                );
                glm::mat4x3 mat4x3_colmajor = scene.gsi_transforms_[i];
                float mat4x4_rowmajor[16] = {
                    mat4x3_colmajor[0][0], mat4x3_colmajor[1][0], mat4x3_colmajor[2][0], mat4x3_colmajor[3][0],
                    mat4x3_colmajor[0][1], mat4x3_colmajor[1][1], mat4x3_colmajor[2][1], mat4x3_colmajor[3][1],
                    mat4x3_colmajor[0][2], mat4x3_colmajor[1][2], mat4x3_colmajor[2][2], mat4x3_colmajor[3][2],
                    0, 0, 0, 1
                };
                gfxRaytracingPrimitiveSetTransform(gfx, device_scene.rt_primitives_[i], mat4x4_rowmajor);
                gfxRaytracingPrimitiveSetInstanceID(gfx, device_scene.rt_primitives_[i], i | RT_INSTANCE_REGULAR_MESH_BIT);
                gfxRaytracingPrimitiveSetInstanceContributionToHitGroupIndex(
                        gfx, device_scene.rt_primitives_[i],
                        0
                );
                gfxDestroyBuffer(gfx, index_range);
                gfxDestroyBuffer(gfx, vertex_range);
            }
        }
        should_rebuild_TLAS_ = true;

        gfxDestroyBuffer(gfx, vertex_buffer);
        gfxDestroyBuffer(gfx, index_buffer);

        should_build_acceleration_structure_ = false;
    }

    if (should_rebuild_TLAS_) {
        for (int i = 0; i < scene.GetNumInstances(); i++) {
            glm::mat4x3 mat4x3_colmajor = scene.gsi_transforms_[i];
            float mat4x4_rowmajor[16] = {
                mat4x3_colmajor[0][0], mat4x3_colmajor[1][0], mat4x3_colmajor[2][0], mat4x3_colmajor[3][0],
                mat4x3_colmajor[0][1], mat4x3_colmajor[1][1], mat4x3_colmajor[2][1], mat4x3_colmajor[3][1],
                mat4x3_colmajor[0][2], mat4x3_colmajor[1][2], mat4x3_colmajor[2][2], mat4x3_colmajor[3][2],
                0, 0, 0, 1
            };
            gfxRaytracingPrimitiveSetTransform(gfx, device_scene.rt_primitives_[i], mat4x4_rowmajor);
        }
        gfxAccelerationStructureUpdate(gfx, device_scene.acceleration_structure_);
        should_rebuild_TLAS_ = false;
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
        auto check = [&] (int i, int x, int y, glm::vec2 tiles) {
            for (int dy = 0; dy < tiles.y; dy++) {
                for (int dx = 0; dx < tiles.x; dx++) {
                    if (MC.atlas_occupancy[i][y + dy][x + dx]) {
                        return false;
                    }
                }
            }
            return true;
        };
        auto find = [&] (glm::ivec2 tiles, int start_i = 0, int start_x = 0, int start_y = 0) {
            int lim_x = (CARD_ATLAS_RESOLUTION / MIN_CARD_RESOLUTION - tiles.x) ;
            int lim_y = (CARD_ATLAS_RESOLUTION / MIN_CARD_RESOLUTION - tiles.y) ;
            for (int i = start_i; i < NUM_CARD_ATLAS; i++) {
                for (int y = start_y; y < lim_y; y++) {
                    for (int x = start_x; x < lim_x; x++) {
                        if (check(i, x, y, tiles)) {
                            return glm::ivec3(x, y, i);
                        }
                    }
                }
            }
            return glm::ivec3(-1, -1, -1);
        };
        auto fill = [&] (int i, int x, int y, glm::vec2 tiles, int fill_value = 1) {
            for (int dy = 0; dy < tiles.y; dy++) {
                for (int dx = 0; dx < tiles.x; dx++) {
                    MC.atlas_occupancy[i][y + dy][x + dx] = fill_value;
                }
            }
        };
        auto find_consecutive_card_headers = [&] (int num_cards) {
            int found_card_start_index = -1;
            for (int i = 0; i < (int)MC.cards.size(); i++) {
                int j = i;
                bool flag = true;
                for (; j < std::min((int)MC.cards.size(), i + num_cards); j++) {
                    if (MC.cards[j].AtlasBaseCoords.x != -1) {
                        flag = false; break;
                    }
                }
                if (flag) {
                    found_card_start_index = i;
                    break;
                } else i = j;
            }
            return found_card_start_index;
        };
        auto allocate_cards = [&] (int card_header_start_index, int num_cards, glm::ivec2 tiles) {
            int atlas_index = 0, i = 0;
            while (i < num_cards && atlas_index < NUM_CARD_ATLAS) {
                auto pos = find(tiles, atlas_index);
                if (pos.x == -1) {
                    atlas_index ++;
                } else {
                    fill(pos.z, pos.x, pos.y, tiles);
                    MC.cards[card_header_start_index + i].AtlasBaseCoords = pos * MIN_CARD_RESOLUTION;
                    i ++;
                    atlas_index = pos.z;
                }
            }
            return i == num_cards;
        };

        // Remove card sets
        std::sort(MC.card_set_remove_requests.begin(), MC.card_set_remove_requests.end());
        MC.card_set_remove_requests.erase(std::unique(MC.card_set_remove_requests.begin(), MC.card_set_remove_requests.end()), MC.card_set_remove_requests.end());
        while (MC.card_set_remove_requests.size() > 0) {
            auto & instance_id = MC.card_set_remove_requests.back();
            MC.card_set_remove_requests.pop_back();
            if (MC.card_sets.size() <= instance_id) {
                app_warning("Unidentified card set to remove.");
                continue;
            }
            auto & card_set = MC.card_sets[instance_id];
            int card_index_base = card_set.CardIndexBase;
            auto resolutions = card_set.CardResolutions;
            // remove cards
            int card_rank = 0;
            for (int ax = 0; ax < 3; ax ++) {
                int s_axis = (ax + 1) % 3;
                int t_axis = (ax + 2) % 3;
                for (int i = 0; i < card_set.NumCards[ax]; i ++) {
                    auto & card = MC.cards[card_index_base + card_rank];
                    if (card.AtlasBaseCoords.x != -1) {
                        // Clean occupancy
                        fill(card.AtlasBaseCoords.z, card.AtlasBaseCoords.x, card.AtlasBaseCoords.y,
                            {resolutions[s_axis], resolutions[t_axis]}, 0);
                    }
                    // remove card header
                    card = {{-1, -1, -1}};
                    card_rank ++;
                }
            }
            // remove set header
            card_set = {};
        }
        // Allocate card sets
        std::sort(MC.card_set_add_requests.begin(), MC.card_set_add_requests.end());
        MC.card_set_add_requests.erase(std::unique(MC.card_set_add_requests.begin(), MC.card_set_add_requests.end()), MC.card_set_add_requests.end());
        while (MC.card_set_add_requests.size() > 0) {
            auto & instance_id = MC.card_set_add_requests.back();
            MC.card_set_add_requests.pop_back();
            auto aabb = scene.GetInstanceAABB(instance_id);
            auto transform = scene.GetInstanceTransform(instance_id);
            glm::vec3 scaling = transform.scale;
            glm::vec3 world_extents = (aabb.mx - aabb.mn) * scaling;
            glm::ivec3 preferred_num_texels = glm::ivec3(world_extents / UB.Card_PreferredTexelWorldSize);
            // Find the actual mip size
            int max_num_texels = glm::max(glm::max(preferred_num_texels.x, preferred_num_texels.y), preferred_num_texels.z);
            static_assert((1<<MIN_CARD_RESOLUTION_L2) == MIN_CARD_RESOLUTION);
            int base_level = MIN_CARD_RESOLUTION_L2;
            while (1 << base_level < max_num_texels) {
                base_level++;
            }
            int max_resolution = glm::min(1 << base_level, MAX_CARD_RESOLUTION);
            float factor = (float)max_resolution / max_num_texels;
            glm::ivec3 texel_counts = glm::ivec3(glm::ceil(glm::vec3(preferred_num_texels) * factor));
            for (int i = 0; i < 3; i++) {
                texel_counts[i] = roundUpPow2(glm::clamp(texel_counts[i], MIN_CARD_RESOLUTION, MAX_CARD_RESOLUTION));
            }
            // TODO allocate multiple cards for each axis adaptively
            glm::ivec3 num_cards = {1, 1, 1};
            // Find and allocate texture atlas for the mesh cards (brute force)
            // The following code produces zeroes. glm substitutes division into multiplying a inversion of integer...
            // which is for certain 0.
            //glm::ivec3 tiles = texel_counts / MIN_CARD_RESOLUTION;
            glm::ivec3 tiles = (1.f / MIN_CARD_RESOLUTION) * glm::vec3(texel_counts);
            int num_cards_to_allocate = 2 * (num_cards.x + num_cards.y + num_cards.z);
            int card_header_offset = find_consecutive_card_headers(num_cards_to_allocate);
            if (card_header_offset == -1) {
                for (int i = 0; i < num_cards_to_allocate; i++) {
                    MC.cards.push_back({{-1, -1, -1}});
                }
                card_header_offset = MC.cards.size() - num_cards_to_allocate;
            }
            // Write card set information and Card headers
            CardSet card_set = {};
            card_set.CardIndexBase = card_header_offset;
            // yz plane
            assert(allocate_cards(card_header_offset, num_cards.x * 2, glm::ivec2(tiles.y, tiles.z)));
            card_header_offset += num_cards.x * 2;
            // zx plane
            assert(allocate_cards(card_header_offset , num_cards.y * 2, glm::ivec2(tiles.z, tiles.x)));
            card_header_offset += num_cards.y * 2;
            // xy plane
            assert(allocate_cards(card_header_offset, num_cards.z * 2, glm::ivec2(tiles.x, tiles.y)));

            card_set.CardResolutions = texel_counts;
            card_set.NumCards = num_cards * 2;
            card_set.MinBounds = aabb.mn;
            card_set.MaxBounds = aabb.mx;

            if (MC.card_sets.size() <= instance_id) {
                MC.card_sets.resize(instance_id + 1);
            }
            MC.card_sets[instance_id] = card_set;
            // Append to redraw list
            MC.card_set_redraw_requests.push_back(instance_id);
        }
        // Upload card headers and cardset headers to GPU
        {
            std::vector<float4> card_sets_packed;
            card_sets_packed.resize(MC.card_sets.size() * 2);
            for (int i = 0; i < (int)MC.card_sets.size(); i++) {
                const auto & card_set = MC.card_sets[i];
                card_sets_packed[i * 2 + 0] = {card_set.MinBounds, card_set.MaxBounds.x};
                uint Z = card_set.CardIndexBase & 0xffffu;
                Z |= (card_set.NumCards.x & 0xfu) << 16;
                Z |= (card_set.NumCards.y & 0xfu) << 20;
                Z |= (card_set.NumCards.z & 0xfu) << 24;
                uint W = uint(glm::log2((float)card_set.CardResolutions.x / MIN_CARD_RESOLUTION)) & 0xfu;
                W |= (uint(glm::log2((float)card_set.CardResolutions.y / MIN_CARD_RESOLUTION)) & 0xfu) << 4;
                W |= (uint(glm::log2((float)card_set.CardResolutions.z / MIN_CARD_RESOLUTION)) & 0xfu) << 8;
                card_sets_packed[i * 2 + 1] = {card_set.MaxBounds.y, card_set.MaxBounds.z,
                    std::bit_cast<float>(Z), std::bit_cast<float>(W)};
            }
            UploadBufferStaged(buf_.card_sets, card_sets_packed.data(), card_sets_packed.size() * sizeof(float4));
            std::vector<uint> cards_packed;
            cards_packed.resize(MC.cards.size());
            for (int i = 0; i < (int)MC.cards.size(); i++) {
                auto & card = MC.cards[i];
                uint X = card.AtlasBaseCoords.x & 0xfffu;
                X |= (card.AtlasBaseCoords.y & 0xfffu) << 12;
                X |= (card.AtlasBaseCoords.z & 0xffu) << 24;
                cards_packed[i] = X;
            }
            UploadBufferStaged(buf_.cards, cards_packed.data(), cards_packed.size() * sizeof(uint));
        }
        // Redraw card sets
        std::sort(MC.card_set_redraw_requests.begin(), MC.card_set_redraw_requests.end());
        MC.card_set_redraw_requests.erase(std::unique(MC.card_set_redraw_requests.begin(), MC.card_set_redraw_requests.end()), MC.card_set_redraw_requests.end());
        while (MC.card_set_redraw_requests.size() > 0) {
            auto & instance_id = MC.card_set_redraw_requests.back();
            MC.card_set_redraw_requests.pop_back();
            if (MC.card_sets.size() <= instance_id) {
                app_warning("Unidentified card set to redraw.");
                continue;
            }
            // no batching, draw 1 card at a time (as the cards are not really updated frequently)
            auto render_card = [&] (int card_index, int card_set_index, glm::ivec3 card_atlas_coords, glm::ivec2 resolution, Camera card_camera) {
                DrawMeshCardUniformBlock MCUB {};
                MCUB.Camera = card_camera.PackDescription(resolution.x, resolution.y, {});

                MCUB.CardAtlasBaseCoords = card_atlas_coords;
                MCUB.InstanceIndex = instance_id;

                MCUB.CardIndex = card_index;
                MCUB.CardSetIndex = card_set_index;
                GfxBuffer MCUB_range = AllocateUBForCurrentFrame<DrawMeshCardUniformBlock>();
                gfxBufferGetData<DrawMeshCardUniformBlock>(gfx, MCUB_range)[0] = MCUB;
                gfxProgramSetParameter(gfx, program_, "MCUB", MCUB_range);
                // Clear card atlas data
                glm::ivec2 card_groups = {divideAndRoundUp(resolution.x, TILE_SIZE), divideAndRoundUp(resolution.y, TILE_SIZE)};
                {
                    auto section = TimedSection(*this, "ClearCard");
                    gfxCommandBindKernel(gfx, kernel_.ClearCard);
                    auto threads = gfxKernelGetNumThreads(gfx, kernel_.ClearCard);
                    gfxCommandDispatch(gfx, card_groups.x, card_groups.y, 1);
                }
                // Rasterization
                {
                    auto section = TimedSection(*this, "ClearCounters");
                    gfxCommandBindKernel(gfx, kernel_.ClearCounters);
                    gfxCommandDispatch(gfx, 1, 1, 1);
                }
                {
                    auto section = TimedSection(*this, "FilterActiveGaussiansForCard");
                    gfxCommandBindKernel(gfx, kernel_.FilterActiveGaussiansForCard);
                    auto threads = gfxKernelGetNumThreads(gfx, kernel_.FilterActiveGaussiansForCard);
                    int num_instance_gaussians = scene.GetInstanceNumGaussians(instance_id);
                    gfxCommandDispatch(gfx, divideAndRoundUp(num_instance_gaussians, (int)threads[0]), 1, 1);
                }
                {
                    auto section = TimedSection(*this, "ProjectActiveGaussiansForCard");
                    GenerateDispatchIndirect(buf_.active_gaussian_count);
                    gfxCommandBindKernel(gfx, kernel_.ProjectActiveGaussiansForCard);
                    gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
                }
                {
                    auto section = TimedSection(*this, "SortActiveGaussiansForCard");
                    gfxCommandSortRadix(gfx, buf_.active_gaussian_linear_depth, buf_.active_gaussian_linear_depth_src,
                                        &buf_.active_gaussian_indirect, &buf_.active_gaussian_indirect_src, &buf_.active_gaussian_count);
                }
                {
                    // Rasterize the G-Buffers
                    auto section = TimedSection(*this, "DrawActiveGaussiansForCard");
                    // Cleared to (0, 0, 0, 0)
                    GenerateDrawIndirect(buf_.active_gaussian_count);
                    gfxCommandClearTexture(gfx, tex_.card_workspace_color_alpha);
                    gfxCommandClearTexture(gfx, tex_.card_workspace_linear_depth);
                    gfxCommandClearTexture(gfx, tex_.card_workspace_normal);
                    gfxCommandBindKernel(gfx, kernel_.DrawActiveGaussiansForCard);
                    gfxCommandBindColorTarget(gfx, 0, tex_.card_workspace_color_alpha);
                    gfxCommandBindColorTarget(gfx, 1, tex_.card_workspace_linear_depth);
                    gfxCommandBindColorTarget(gfx, 2, tex_.card_workspace_normal);
                    gfxCommandSetViewport(gfx, 0, 0, resolution.x, resolution.y);

        #ifndef NO_INDIRECT_DISPATCH
                    gfxCommandMultiDrawIndirect(gfx, buf_.draw_indirect_command, 1);
        #endif
                    gfxCommandSetViewport(gfx);
                }

                {
                    // Resolve GBuffers rendered with gaussians. Doing some normalizations for weighted sums.
                    auto section = TimedSection(*this, "ResolveGBuffersForCard");
                    gfxCommandBindKernel(gfx, kernel_.ResolveGBuffersForCard);
                    auto num_threads = gfxKernelGetNumThreads(gfx, kernel_.ResolveGBuffersForCard);
                    assert(UB.ScreenDimensions.x % num_threads[0] == 0 && UB.ScreenDimensions.y % num_threads[1] == 0);
                    uint num_groups_x = divideAndRoundUp((uint)UB.ScreenDimensions.x, num_threads[0]);
                    uint num_groups_y = divideAndRoundUp((uint)UB.ScreenDimensions.y, num_threads[1]);
                    gfxCommandDispatch(gfx, num_groups_x, num_groups_y, 1);
                }
                {
                    // Now, combine the G-Buffers rendered with gaussians and the ordinary G-Buffers
                    auto section = TimedSection(*this, "CopyCardToAtlas");
                    gfxCommandBindKernel(gfx, kernel_.CopyCardToAtlas);
                    gfxCommandDispatch(gfx, card_groups.x, card_groups.y, 1);
                }
            };
            auto render_cards = [&] (int card_set_index) {
                CardSet & card_set = MC.card_sets[card_set_index];
                auto num_cards = glm::ivec3(0.5f * glm::vec3(card_set.NumCards));
                auto aabb = scene.GetInstanceAABB(instance_id);
                float offset = 0.05f;
                int card_rank = 0;
                for (int axis = 0; axis < 3; axis ++) {
                    int s_axis = (axis + 1) % 3;
                    int t_axis = (axis + 2) % 3;
                    glm::ivec2 card_resolution = {card_set.CardResolutions[s_axis], card_set.CardResolutions[t_axis]};
                    for (int direction_sgn = 0; direction_sgn < 2; direction_sgn ++) {
                        for (int i = 0; i < num_cards[axis]; i++) {
                            Camera card_camera {};
                            card_camera.direction = {};
                            card_camera.direction[axis] = direction_sgn ? -1 : 1;
                            card_camera.up = {};
                            card_camera.up[t_axis] = 1;
                            float step = 1.f / num_cards[axis];
                            float t = i * step;
                            float t_next = (i + 1 + offset) * step;
                            if (direction_sgn) t = 1 - t, t_next = 1 - t_next;
                            float near_abs = glm::mix(aabb.mn[axis], aabb.mx[axis], t);
                            float far_abs = glm::mix(aabb.mn[axis], aabb.mx[axis], glm::saturate(t_next));
                            card_camera.near = abs(near_abs - (direction_sgn ? aabb.mx[axis] : aabb.mn[axis]));
                            card_camera.far = abs(far_abs - (direction_sgn ? aabb.mx[axis] : aabb.mn[axis]));
                            // FIXME here handness may change, so the clip space may change directions...
                            card_camera.min_clip = {aabb.mn[s_axis], aabb.mn[t_axis]};
                            card_camera.max_clip = {aabb.mx[s_axis], aabb.mx[t_axis]};
                            card_camera.position = {};
                            card_camera.position[axis] = direction_sgn ? aabb.mx[axis] : aabb.mn[axis];
                            card_camera.type = CameraType::eOrthographic;
                            int card_index = card_set.CardIndexBase + card_rank;
                            Card & card = MC.cards[card_index];
                            render_card(card_index, card_set_index, card.AtlasBaseCoords, card_resolution, card_camera);
                            card_rank ++;
                        }
                    }
                }
            };
            render_cards(instance_id);
        }
    }

    {
        auto section = TimedSection(*this, "ClearTextures");
        gfxCommandClearTexture(gfx, tex_.G_albedo_alpha);
        gfxCommandClearTexture(gfx, tex_.debug);
        gfxCommandClearTexture(gfx, tex_.radiance[frame_index_ & 1]);
        gfxCommandClearTexture(gfx, tex_.direct_illumination[frame_index_ & 1]);
        gfxCommandClearTexture(gfx, tex_.indirect_illumination[frame_index_ & 1]);
        gfxCommandClearTexture(gfx, tex_.filtered_direct_illumination);
        gfxCommandClearTexture(gfx, tex_.filtered_indirect_illumination);
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
                gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Miss, 0, "Trace3DGSStochasticMiss");
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
            auto section = TimedSection(*this, "DrawRegularMeshes");
            gfxCommandClearTexture(gfx, tex_.G_albedo_alpha);
            gfxCommandClearTexture(gfx, tex_.G_emission_alpha);
            gfxCommandClearTexture(gfx, tex_.G_material);
            gfxCommandClearTexture(gfx, tex_.G_normal[frame_index_ & 1]);
            gfxCommandClearTexture(gfx, tex_.rasterization_depth);
            gfxCommandBindVertexBuffer(gfx, device_scene.gsi_vertices_);
            gfxCommandBindIndexBuffer(gfx, device_scene.gsi_indices_);
            gfxCommandBindKernel(gfx, kernel_.DrawRegularMeshes);
            // Alpha channel is not used in this draw
            gfxCommandBindColorTarget(gfx, 0, tex_.G_albedo_alpha);
            // Alpha is drawn to this texture (0 or 1)
            gfxCommandBindColorTarget(gfx, 1, tex_.G_emission_alpha);
            gfxCommandBindColorTarget(gfx, 2, tex_.G_material);
            gfxCommandBindColorTarget(gfx, 3, tex_.G_normal[frame_index_ & 1]);
            gfxCommandBindDepthStencilTarget(gfx, tex_.rasterization_depth);
            gfxCommandBindKernel(gfx, kernel_.DrawRegularMeshes);
            for (int i = 0; i < scene.GetNumInstances(); i++) {
                auto instance = scene.GetInstance(i);
                if (instance.type != InstanceType::eMesh) continue ;
                gfxCommandDrawIndexed(gfx, instance.num_indices, 1, instance.index_offset, instance.vertex_offset, i);
            }
        }

        {
            // Filter active gaussians, crop gaussians outside the view frustrum
            auto section = TimedSection(*this, "FilterActiveGaussians");
            is_instance_active_.resize(scene.GetNumInstances());
            gfxCommandBindKernel(gfx, kernel_.FilterActiveGaussians);
            for (int i = 0; i < scene.GetNumInstances(); i++) {
                auto instance = scene.GetInstance(i);
                bool should_draw = is_instance_active_[i];
                if (instance.type == InstanceType::eGaussians && should_draw) {
                    gfxProgramSetParameter(gfx, program_, "g_FilterActiveGaussians_CurrentInstanceIndex", i);
                    gfxCommandDispatch(gfx, divideAndRoundUp(instance.num_vertices, cfg_.wave_lane_count), 1, 1);
                }
            }
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
            // Sort active gaussians according to their depth values.
            // So we can later draw them in order.
            auto section = TimedSection(*this, "SortActiveGaussians");
            gfxCommandSortRadix(gfx, buf_.active_gaussian_linear_depth, buf_.active_gaussian_linear_depth_src,
                                &buf_.active_gaussian_indirect, &buf_.active_gaussian_indirect_src, &buf_.active_gaussian_count);
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
                gfxCommandBindColorTarget(gfx, 3, tex_.G_normal[frame_index_ & 1]);
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
            gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Miss, 0, "Trace3DGSStochasticMiss");

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

        // Indirect lighting phase!

        if (should_reset_hash_grids_) {
            auto section = TimedSection(*this, "SSRC_ResetHashGrids");
            gfxCommandBindKernel(gfx, kernel_.SSRC_ResetHashGrids);
            auto threads = gfxKernelGetNumThreads(gfx, kernel_.SSRC_ResetHashGrids);
            gfxCommandDispatch(gfx, divideAndRoundUp(options_.HashGrids_max_num_tiles, (int)threads[0]), 1, 1);
            should_reset_hash_grids_ = false;
        }

        // Temporal reuse and drop outdated entries for hash grid radiance cache
        {
            auto section = TimedSection(*this, "SSRC_ReInsertHashGridTiles");
            // Clear hash table
            gfxCommandClearBuffer(gfx, buf_.HashGrids_bucket_hash);
            // Re-insert (and filter outdated) hash grid tiles from the last frame
            GenerateDispatchIndirect(buf_.HashGrids_active_tile_count[!(frame_index_ & 1)]);
            gfxCommandBindKernel(gfx, kernel_.SSRC_ReInsertHashGridTiles);
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
#endif
        }

        // Allocate SSRC uniform screen probes
        {
            auto section = TimedSection(*this, "SSRC_AllocateUniformProbes");
            gfxCommandBindKernel(gfx, kernel_.SSRC_AllocateUniformProbes);
            auto threads = gfxKernelGetNumThreads(gfx, kernel_.SSRC_AllocateUniformProbes);
            gfxCommandDispatch(gfx, divideAndRoundUp(UB.SSRC_NumUniformScreenProbes, threads[0]), 1, 1);
        }

        // Allocate SSRC adaptive screen probes in regions with complex geometries
        {
            for(int layer = 0; layer < SSRC_MAX_ADAPTIVE_PROBE_LAYERS; layer ++)
            {
                auto name = std::string("SSRC_AllocateAdaptiveProbes, Layer: ") + std::to_string(layer);
                auto section = TimedSection(*this, name.c_str());

                gfxCommandBindKernel(gfx, kernel_.SSRC_AllocateAdaptiveProbes[layer]);
                auto     threads         = gfxKernelGetNumThreads(gfx, kernel_.SSRC_AllocateAdaptiveProbes[layer]);
                uint32_t tile_count = (4 << (layer * 2)) * UB.SSRC_NumUniformScreenProbes;
                gfxCommandDispatch(gfx, divideAndRoundUp(tile_count, threads[0]), 1, 1);
            }
        }

        // Initialize indirect dispatch commands for probe processing, as well as finalize some counters.
        {
            auto section = TimedSection(*this, "SSRC_PrepareProbeProcessing");
            gfxCommandBindKernel(gfx, kernel_.SSRC_PrepareProbeProcessing);
            gfxCommandDispatch(gfx, 1, 1, 1);
        }

        // Probe history reuse from previous frame.
        // Probes are initialized with reprojected radiances
        {
            auto section = TimedSection(*this, "SSRC_ReprojectProbeHistory");
            gfxCommandBindKernel(gfx, kernel_.SSRC_ReprojectProbeHistory);
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.probe_dispatch_command);
#endif
        }

        // Allocate update rays for each probe. Ray count is proportional to probe reprojection trust
        {
            auto section = TimedSection(*this, "SSRC_AllocateProbeUpdateRays");
            gfxCommandBindKernel(gfx, kernel_.SSRC_AllocateProbeUpdateRays);
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.probe_per_lane_dispatch_command);
#endif
        }

         // Scan sum the ray counts to allocate indices for each probe update ray
         {
             auto section = TimedSection(*this, "ScanSumProbeUpdateRayCounts");
             gfxCommandScanSum(gfx, kGfxDataType_Uint, buf_.probe_update_ray_offsets, buf_.probe_update_ray_counts, &buf_.probe_update_ray_reduce_count);
         }

         // Finalize counters
         {
             auto section = TimedSection(*this, "SSRC_SetRayCounts");
             gfxCommandBindKernel(gfx, kernel_.SSRC_SetRayCounts);
             gfxCommandDispatch(gfx, 1, 1, 1);
         }

        // Importance sample probe update rays using the reprojected radiance distribution on each probe
        {
            auto section = TimedSection(*this, "SSRC_SampleProbeUpdateRays");
            gfxCommandBindKernel(gfx, kernel_.SSRC_SampleProbeUpdateRays);
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.probe_dispatch_command);
#endif
        }

        // Trace sampled rays (stochastic shading rays)
        // Screen tracing
        {
            auto section = TimedSection(*this, "TraceRaysInScreenSpaceForSSRC");
            GenerateDispatchIndirect(buf_.probe_all_update_ray_count);
            gfxCommandBindKernel(gfx, kernel_.TraceRaysInScreenSpaceForSSRC);
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
#endif
        }

        CompactRayTraces();

        // World space HWRT
        {
            auto section = TimedSection(*this, "Trace3DGSProbeUpdateRays");
            GenerateDispatchRaysIndirect(buf_.ray_to_trace_count[ray_compact_count & 1]);
            gfxCommandBindKernel(gfx, kernel_.Trace3DGSProbeUpdateRays);
            gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Raygen, 0, "Trace3DGSProbeUpdateRaysRaygen");
            gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Hit, 0, "Trace3DGSStochasticHitGroup");
            gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Miss, 0, "Trace3DGSStochasticMiss");

#if !(defined(NO_INDIRECT_DISPATCH) || defined(NO_RAYTRACING_INDIRECT_DISPATCH))
            gfxCommandDispatchRaysIndirect(gfx, sbt_, buf_.dispatch_rays_indirect_command);
#endif
        }

        // Resolve ray depth values from probe update ray hits
        {
            auto section = TimedSection(*this, "SSRC_ResolveRayDepths");
            // All rays shall be resolved
            GenerateDispatchIndirect(buf_.probe_all_update_ray_count);
            gfxCommandBindKernel(gfx, kernel_.SSRC_ResolveRayDepths);
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
#endif
        }

        // Try to resolve hit lighting from previous frame's film and skip shading if possible
        {
            auto section = TimedSection(*this, "SSRC_ResolveHitLightingFromScreenHistory");
            // Only resolve the hits that are not found by screen tracing
            GenerateDispatchIndirect(buf_.ray_to_trace_count[ray_compact_count & 1]);
            gfxCommandBindKernel(gfx, kernel_.SSRC_ResolveHitLightingFromScreenHistory);
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
#endif
        }

        // Shade hits that failed to resolve lighting from screen history
        // Spawn light rays for their shading
        {
            auto section = TimedSection(*this, "SSRC_SampleLightRays");
            // Keep a record of the hash grid tile count before allocating new ones
            gfxCommandCopyBuffer(gfx, buf_.HashGrids_active_tile_count_before_allocation, buf_.HashGrids_active_tile_count[frame_index_ & 1]);
            // Clear ray to trace count buffer for allocation
            gfxCommandClearBuffer(gfx, buf_.ray_to_trace_count[ray_compact_count & 1]);
            // Spawn 1 shadow ray for each hit that failed to resolve lighting from screen history
            GenerateDispatchIndirect(buf_.probe_update_ray_hit_shade_count);
            gfxCommandBindKernel(gfx, kernel_.SSRC_SampleLightRays);
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
#endif
        }

        {
            auto section = TimedSection(*this, "SSRC_PrepareClearNewHashGridTileCells");
            gfxCommandBindKernel(gfx, kernel_.SSRC_PrepareClearNewHashGridTileCells);
            gfxCommandDispatch(gfx, 1, 1, 1);
        }

        // Clear cells and update buffers for the newly allocated hash grids (in SSRC_SampleLightRays)
        {
            auto section = TimedSection(*this, "SSRC_ClearNewHashGridTileCells");
            gfxCommandBindKernel(gfx, kernel_.SSRC_ClearNewHashGridTileCells);
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
#endif
        }

        // Trace shadow rays (HWRT)
        {
            auto section = TimedSection(*this, "Trace3DGSShadowRaysWithoutIndirectionList");
            GenerateDispatchRaysIndirect(buf_.ray_to_trace_count[ray_compact_count & 1]);
            gfxCommandBindKernel(gfx, kernel_.Trace3DGSShadowRaysWithoutIndirectionList);
            gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Raygen, 0, "Trace3DGSShadowRaygen");
            gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Hit, 0, "Trace3DGSShadowHitGroup");
            gfxSbtSetShaderGroup(gfx, sbt_, kGfxShaderGroupType_Miss, 0, "Trace3DGSStochasticMiss");
#if !(defined(NO_INDIRECT_DISPATCH) || defined(NO_RAYTRACING_INDIRECT_DISPATCH))
            gfxCommandDispatchRaysIndirect(gfx, sbt_, buf_.dispatch_rays_indirect_command);
#endif
        }

        // Use light samples and trace results to resolve direct lighting and accumulate them into hash grids
        {
            auto section = TimedSection(*this, "SSRC_ResolveHitDirectLightingFromTraceResult");
            GenerateDispatchIndirect(buf_.ray_to_trace_count[ray_compact_count & 1]);
            gfxCommandBindKernel(gfx, kernel_.SSRC_ResolveHitDirectLightingFromTraceResult);
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
#endif
        }

        // Update and filter modified tiles in the hash grid cache
        {
            auto section = TimedSection(*this, "SSRC_FilterHashGrids");
            // Dispatch a thread group for each tile to update
            gfxCommandCopyBuffer(gfx, buf_.dispatch_indirect_command,
                offsetof(DispatchIndirectCommand, ThreadGroupCountX),
                buf_.HashGrids_update_tile_count, 0, sizeof(uint32_t)
            );
            gfxCommandBindKernel(gfx, kernel_.SSRC_FilterHashGrids);
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
#endif
        }

        // Finally, resolve shading results from the cache for probe update rays
        {
            auto section = TimedSection(*this, "SSRC_ResolveProbeUpdateRayRadianceFromCells");
            // Each hit outside the screen history potentially requires a resolve
            GenerateDispatchIndirect(buf_.probe_update_ray_hit_shade_count);
            gfxCommandBindKernel(gfx, kernel_.SSRC_ResolveProbeUpdateRayRadianceFromCells);
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.dispatch_indirect_command);
#endif
        }

        // Use probe update ray trace results to update SSRC probes
        {
            auto section = TimedSection(*this, "SSRC_UpdateProbes");
            gfxCommandBindKernel(gfx, kernel_.SSRC_UpdateProbes);
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.probe_dispatch_command);
#endif
        }

        // Filter probes as well as calculate SH projection
        {
            auto section = TimedSection(*this, "SSRC_FilterProbes");
            gfxCommandBindKernel(gfx, kernel_.SSRC_FilterProbes);
            // Only filter uniform screen probes
            gfxCommandDispatch(gfx, UB.SSRC_NumUniformScreenProbes, 1, 1);
        }

        // Make a padded probe atlas for hardware accelerated interpolation in later stages
        {
            auto section = TimedSection(*this, "SSRC_PadProbeTextureEdges");
            gfxCommandBindKernel(gfx, kernel_.SSRC_PadProbeTextureEdges);
#ifndef NO_INDIRECT_DISPATCH
            gfxCommandDispatchIndirect(gfx, buf_.probe_dispatch_command);
#endif
        }

        // Integrate indirect illumination for diffuse BRDF from SSRC
        {
            auto section = TimedSection(*this, "SSRC_Integrate");
            gfxCommandBindKernel(gfx, kernel_.SSRC_Integrate);
            gfxCommandDispatch(gfx, UB.TileDimensions.x, UB.TileDimensions.y, 1);
        }

        // Temporal denoising for direct and indirect illumination
        {
            auto section = TimedSection(*this, "TemporalDenoiseLighting");
            gfxCommandBindKernel(gfx, kernel_.TemporalDenoiseLighting);
            gfxCommandDispatch(gfx, UB.TileDimensions.x, UB.TileDimensions.y, 1);

            gfxCommandCopyTexture(gfx, tex_.direct_illumination[frame_index_ & 1], tex_.filtered_direct_illumination);
            gfxCommandCopyTexture(gfx, tex_.indirect_illumination[frame_index_ & 1], tex_.filtered_indirect_illumination);
        }

        if (UB.Debug_VisualizeMeshCardScene) {
            auto section = TimedSection(*this, "VisualizeMeshCardScene");
            gfxCommandBindKernel(gfx, kernel_.VisualizeMeshCardScene);
            gfxCommandDispatch(gfx, UB.TileDimensions.x, UB.TileDimensions.y, 1);
        }

        if (UB.Debug_VisualizeMeshCardAtlas) {
            auto section = TimedSection(*this, "VisualizeMeshCardAtlas");
            gfxCommandBindKernel(gfx, kernel_.VisualizeMeshCardAtlas);
            gfxCommandDispatch(gfx, UB.TileDimensions.x, UB.TileDimensions.y, 1);
        }

        // Final radiance composition
        {
            auto section = TimedSection(*this, "FinalComposition");
            gfxCommandClearTexture(gfx, tex_.history_radiance_without_emission);
            gfxCommandBindKernel(gfx, kernel_.FinalComposition);
            gfxCommandDispatch(gfx, UB.TileDimensions.x, UB.TileDimensions.y, 1);
        }

        if (UB.Debug_SSRC_VisualizeProbes) {
            auto section = TimedSection(*this, "Debug_SSRC_VisualizeProbes");
            gfxCommandCopyTexture(gfx, tex_.debug, tex_.radiance[frame_index_ & 1]);
            gfxCommandBindKernel(gfx, kernel_.Debug_SSRC_VisualizeProbes);
            auto threads = gfxKernelGetNumThreads(gfx, kernel_.Debug_SSRC_VisualizeProbes);
            gfxCommandDispatch(gfx, divideAndRoundUp(UB.SSRC_NumUniformScreenProbes, threads[0]), 1, 1);
        }

        if (UB.Debug_SSRC_VisualizeProbeUpdateRays) {
            auto section = TimedSection(*this, "Debug_SSRC_VisualizeProbeUpdateRays");
            gfxCommandCopyTexture(gfx, tex_.debug, tex_.radiance[frame_index_ & 1]);
            gfxCommandBindKernel(gfx, kernel_.Debug_SSRC_PrepareVisualizeProbeUpdateRays);
            gfxCommandDispatch(gfx, 1, 1, 1);
            gfxCommandBindKernel(gfx, kernel_.Debug_SSRC_VisualizeProbeUpdateRays);
            gfxCommandBindColorTarget(gfx, 0, tex_.debug);
            gfxCommandBindDepthStencilTarget(gfx, tex_.rasterization_depth);
            gfxCommandMultiDrawIndirect(gfx, buf_.draw_indirect_command, 1); // draw to debug texture
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

void Renderer::RequestRedrawAllMeshCardsForAllInstances() {
    for (int i = 0; i < (int)MC.card_sets.size(); i++) {
        MC.card_set_remove_requests.push_back(i);
    }
    auto & scene = AppInternal::GetInstance().GetScene();
    for (int i = 0; i < scene.GetNumInstances(); i++) {
        MC.card_set_add_requests.push_back(i);
    }
}

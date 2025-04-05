/*
 * Created: 2024/11/7
 * Author:  hineven
 * See LICENSE for licensing.
 */

#include <iostream>
#include "app_internal.h"

#include <fstream>

#include "renderer.h"
#include "gfx_imgui.h"
#include "glm/detail/type_quat.hpp"
#include "glm/ext/quaternion_trigonometric.hpp"
#include "glm/gtx/quaternion.hpp"
#include "glm/gtx/rotate_vector.hpp"

AppInternal::AppInternal() {}

AppInternal::~AppInternal() {}


static AppInternal * GAppInternalInstance;

int AppInternal::Run () {
    // Create window and gfx context
    window_ = gfxCreateWindow(GetWindowWidth(), GetWindowHeight());
    gfx_ = gfxCreateContext(
            window_
#ifndef NDEBUG
            // ,
            // kGfxCreateContextFlag_EnableShaderDebugging
            // | kGfxCreateContextFlag_EnableDebugLayer
            // | kGfxCreateContextFlag_EnableStablePowerState
#endif
    );

    glm::vec3 absolute_up = glm::vec3(0, 1, 0);

    // ImGui
    {
        // Create ImGui context using additional needed fonts
        char const  *fonts[] = {"C:\\Windows\\Fonts\\seguisym.ttf"};
        ImFontConfig fontConfigs[1];
        fontConfigs[0].MergeMode           = true;
        static const ImWchar glyphRanges[] = {
                0x2310,
                0x23FF, // Media player icons
                0x1F500,
                0x1F505, // Restart icon
                0,
        };
        fontConfigs[0].GlyphRanges = &glyphRanges[0];
        fontConfigs[0].SizePixels  = 30.0f;
        fontConfigs[0].GlyphOffset.y += 5.0f; // Need to offset glyphs downward to properly center them
        if (auto err = gfxImGuiInitialize(gfx_, fonts, 1, fontConfigs); err != kGfxResult_NoError)
        {
            std::cerr << "Failed to initialize ImGui: " << err << std::endl;
            return 1;
        }
    }


    // Detect shader path
    std::string root_path = "";
    bool found = false;
    for (int depth = 0; depth < 4; depth++) {
        if (std::filesystem::exists(root_path + "src/device_shared.hlsl")) {
            found = true;
            break;
        }
        root_path += "../";
    }
    if (!found) {
        std::cerr << "Cannot find shader path" << std::endl;
        return 1;
    }
    // Get the absolute path
    root_path = std::filesystem::absolute(root_path).string();
    std::cout << "Shader path: " << root_path << std::endl;
    root_path_ = root_path;

    // Create samplers
    {
        samplers_.linear_clamp = gfxCreateSamplerState(gfx_, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
        samplers_.linear_wrap = gfxCreateSamplerState(gfx_, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                                                      D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
        samplers_.point_clamp = gfxCreateSamplerState(gfx_, D3D12_FILTER_MIN_MAG_MIP_POINT);
        samplers_.point_wrap = gfxCreateSamplerState(gfx_, D3D12_FILTER_MIN_MAG_MIP_POINT,
                                                     D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    }

    // Load scene
    {
        LoadRaw3DGSScene("garden");

        // scene_.LoadGltf(root_path + "data/chinese_dragon/scene.gltf");
        // LoadTeaserScene("rogland_overcast_4k.exr", true);

        // LoadLightingComparisonScene("waymo1442", "rogland_overcast_4k.exr");
        // LoadLightingComparisonScene("armadillo_pbr", "rogland_overcast_4k.exr");
        // LoadLightingComparisonScene("chair", "qwantani_dusk_2_4k.exr");
        // LoadLightingComparisonScene("jugs", "qwantani_dusk_2_4k.exr");
        // LoadLightingComparisonScene("hotdog", "qwantani_dusk_2_4k.exr");
        // LoadLightingComparisonScene("caterpillar", "qwantani_dusk_2_4k.exr", {-90, 0, 0});
        // LoadLightingComparisonScene("drums", "rogland_overcast_4k.exr");
        // LoadLightingComparisonScene("nerf_chair", "rogland_overcast_4k.exr");
        // LoadLightingComparisonScene("barn", "rogland_overcast_4k.exr", {90, 0, 0}, {0.7, 0.7, 0.7});
        // LoadLightingComparisonScene("family", "tief_etz_4k.exr", {-90, 0, 0}, {0.7, 0.7, 0.7});
        // LoadLightingComparisonScene("ficus", "rogland_overcast_4k.exr");
        // LoadLightingComparisonScene("armadillo", "rogland_overcast_4k.exr");
        // LoadArmadilloMeshScene("rogland_overcast_4k.exr");
        // LoadMultiModelLightingComparisonScene("tief_etz_4k.exr");
        // LoadFaultyArmadilloScene();
        // LoadCornellBoxScene("family", {-90, 36, 0});
        // LoadAllLightsScene();
        // LoadAllLightsScene(true);
        // LoadMeshGaussianTransportScene("rogland_overcast_4k");
        // LoadLightRoomScene("air_baloons",  "overcast_soil_4k", glm::vec3(-0.4, 1.6, 0.4));
        // LoadLightRoomScene("armadillo", "tief_etz_4k");


        scene_.UpdateSceneBounds();
        scene_.UpdateDeviceScene();
    }


    auto renderer = std::make_unique<Renderer>();

    renderer->Initialize();

    auto clock = std::chrono::high_resolution_clock();

    double last_frame_time = std::chrono::duration<double>(clock.now().time_since_epoch()).count();
    double beginning_time = last_frame_time;

    float delta_tick = 1.f;
    std::vector<std::pair<std::string, float>> last_frame_timed_sections;
    std::vector<float> frame_latency_history;

    bool show_ui = true;

    bool auto_rotate = false;//true;

    glm::vec3 start_camera_position = scene_.GetCamera().position;
    glm::vec3 start_camera_direction = scene_.GetCamera().direction;

    // Main loop
    while(!gfxWindowIsCloseRequested(window_)) {
        gfxWindowPumpEvents(window_);

        // Op flags
        bool need_reload_shaders = false;

        // Timer
        float frame_total_time = 0.f;
        {
            {
                for(auto & section : last_frame_timed_sections) {
                    frame_total_time += section.second;
                }
                frame_latency_history.push_back(frame_total_time);
                if (frame_latency_history.size() > 100) {
                    frame_latency_history.erase(frame_latency_history.begin());
                }
            }
        }

        // UI (Logic)
        if (show_ui) {
            ImGui::Begin("3DGS AdvGI");
            if(ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto & camera = scene_.GetCamera();
                ImGui::Text("Position: %.2f %.2f %.2f", camera.position.x, camera.position.y, camera.position.z);
                ImGui::Text("Direction: %.2f %.2f %.2f", camera.direction.x, camera.direction.y, camera.direction.z);
                ImGui::Text("Up: %.2f %.2f %.2f", camera.up.x, camera.up.y, camera.up.z);
                ImGui::Text("FOV: %.2f", camera.fov_y);
                ImGui::Text("Near: %.2f", camera.near);
                ImGui::Text("Far: %.2f", camera.far);
            }
            ImGui::Separator();
            if(ImGui::Button("Reload shaders (F5)")) {
                need_reload_shaders = true;
            }
            if(ImGui::CollapsingHeader("Profile")) {
                for(auto & section : last_frame_timed_sections) {
                    // Keep the texts aligned
                    std::string name = section.first;
                    if(name.size() < 30) {
                        name += std::string(30 - name.size(), ' ');
                    }
                    ImGui::Text("- %s: %4.2f ms", name.c_str(), section.second);
                }
                ImGui::Text("Total: %4.2f ms", frame_total_time);
                ImGui::Text("CPU Delta Tick: %4.2f ms", delta_tick * 1000.f);
                float fps = 1000.f / frame_total_time;
                float minfps = fps, maxfps = fps;
                for(auto & latency : frame_latency_history) {
                    minfps = std::min(minfps, 1000.f / latency);
                    maxfps = std::max(maxfps, 1000.f / latency);
                }
                char overlay[128];
                sprintf_s(overlay, "FPS: %4.2f, Min: %4.2f, Max: %4.2f", fps, minfps, maxfps);
                ImGui::PlotLines("Latency", frame_latency_history.data(), frame_latency_history.size(),
                    0, overlay, 0.f);
            }

            renderer->RenderUI();
            ImGui::End();
        }

        // Render
        renderer->Render();

        // Update timed sections
        last_frame_timed_sections = renderer->CollectTimedSections();

        // Submit the frame (UI
        {
            GfxCommandEvent const command_event(gfx_, "DrawImGui");
            gfxImGuiRender();
        }

        // End of device frame

        gfxFrame(gfx_, false);

        // Logic
        double this_frame_time = std::chrono::duration<double>(clock.now().time_since_epoch()).count();
        delta_tick = float(this_frame_time - last_frame_time);
        last_frame_time = this_frame_time;
        double time_elapsed = this_frame_time - beginning_time;

        // Camera navigation
        {
            auto & camera = scene_.GetCamera();
            float const camera_speed = 1.f;
            if (gfxWindowIsKeyDown(window_, VK_SHIFT))
            {
                camera_speed_ *= (float)exp(delta_tick * log(1.002));
            }
            if (gfxWindowIsKeyDown(window_, VK_CONTROL)) {
                camera_speed_ /= (float)exp(delta_tick * log(1.002));
            }
            if (gfxWindowIsKeyDown(window_, 'W'))
            {
                camera.position += camera.direction * camera_speed * delta_tick;
            }
            if (gfxWindowIsKeyDown(window_, 'S'))
            {
                camera.position -= camera.direction * camera_speed * delta_tick;
            }
            if (gfxWindowIsKeyDown(window_, 'A'))
            {
                camera.position -= glm::cross(camera.direction, camera.up) * camera_speed * delta_tick;
            }
            if (gfxWindowIsKeyDown(window_, 'D'))
            {
                camera.position += glm::cross(camera.direction, camera.up) * camera_speed * delta_tick;
            }
            // Roll
            float roll = 0.f;
            if(gfxWindowIsKeyDown(window_, 'Q')) {
                roll += 1.f;
            }
            if(gfxWindowIsKeyDown(window_, 'E')) {
                roll -= 1.f;
            }
            if (roll != 0.f)
            {
                glm::quat rotation = glm::angleAxis(roll * delta_tick * 0.4f, camera.direction);
                absolute_up = glm::normalize(rotation * absolute_up);
                camera.up = glm::normalize(rotation * camera.up);
            }
            auto acceleration = glm::vec2(0.0f);
            if (!ImGui::GetIO().WantCaptureMouse)
            {
                acceleration.x -= ImGui::GetMouseDragDelta(0, 0.0f).x;
                acceleration.y -= ImGui::GetMouseDragDelta(0, 0.0f).y;
            }
            glm::vec2 rotation = acceleration * delta_tick * 0.4f;
            rotation = glm::clamp(rotation, -4e-2f, 4e-2f);
            // Clamp tiny values to zero to improve convergence to resting state
            auto const clampRotationMin = glm::lessThan(glm::abs(rotation), glm::vec2(0.00000001f));
            if (glm::any(clampRotationMin))
            {
                if (clampRotationMin.x)
                {
                    rotation.x = 0.0f;
                }
                if (clampRotationMin.y)
                {
                    rotation.y = 0.0f;
                }
            }
            ImGui::ResetMouseDragDelta(0);

            if (!glm::all(glm::equal(rotation, glm::vec2(0.0f))))
            {
                // Update translation

                glm::vec3 right = glm::normalize(glm::cross(camera.direction, absolute_up));
                glm::vec3 up    = glm::normalize(glm::cross(right, camera.direction));

                // Rotate camera
                glm::quat rotationX = glm::angleAxis(-rotation.x, absolute_up);
                glm::quat rotationY = glm::angleAxis(-rotation.y, right);

                const glm::vec3 newForward = normalize(camera.direction * rotationX * rotationY);
                if (abs(dot(newForward, glm::vec3(0.0f, 1.0f, 0.0f))) < 0.9f)
                {
                    // Prevent view and up direction becoming parallel (this uses a FPS style camera)
                    camera.direction   = newForward;
                    const glm::vec3 newRight = normalize(cross(camera.direction, absolute_up));
                    camera.up          = normalize(cross(newRight, newForward));
                }
            }
        }

        if (auto_rotate) {
            // Rotate the camera for simple object relighting visualization
            auto & camera = scene_.GetCamera();
            glm::vec3 rotation = glm::vec3(0, 0.8 * time_elapsed, 0);
            glm::vec3 rotated_position = glm::rotateY(start_camera_position, rotation.y);
            glm::vec3 rotated_direction = glm::rotateY(start_camera_direction, rotation.y);
            camera.position = rotated_position;
            camera.direction = rotated_direction;
        }

        // Hot-reload the shaders if requested
        if (gfxWindowIsKeyReleased(window_, VK_F5) || need_reload_shaders)
        {
            gfxFinish(gfx_);
            renderer->Destroy();
            app_assert(renderer->Initialize());
            std::cout << "Shaders reloaded" << std::endl;
        }

        // Screen capture
        if (gfxWindowIsKeyPressed(window_, VK_F2)) {
            renderer->ScreenShot("screenshot.png");
        }

        if (gfxWindowIsKeyReleased(window_, VK_F3)) {
            show_ui = !show_ui;
        }

        if (gfxWindowIsKeyReleased(window_, VK_F4)) {
            std::fstream file("profile.txt", std::ios::out);
            for(auto & section : last_frame_timed_sections) {
                // Keep the texts aligned
                std::string name = section.first;
                if(name.size() < 30) {
                    name += std::string(30 - name.size(), ' ');
                }
                char buf[128];
                sprintf_s(buf, "%s: %4.2f ms\n", name.c_str(), section.second);
                file << buf;
            }
        }
    }

    gfxFinish(gfx_);


    // Destroy renderer
    renderer->Destroy();
    renderer.reset();

    // Destroy scene
    scene_.DestroyDeviceScene();

    gfxImGuiTerminate();

    // Destroy samplers
    gfxDestroySamplerState(gfx_, samplers_.linear_clamp);
    gfxDestroySamplerState(gfx_, samplers_.linear_wrap);
    gfxDestroySamplerState(gfx_, samplers_.point_clamp);
    gfxDestroySamplerState(gfx_, samplers_.point_wrap);

    // Destroy gfx context and window
    gfxDestroyContext(gfx_);
    gfxDestroyWindow(window_);

    return 0;
}

AppInternal &AppInternal::GetInstance() {
    return *GAppInternalInstance;
}

void AppInternal::SetSingleton(AppInternal *singleton) {
    GAppInternalInstance = singleton;
}


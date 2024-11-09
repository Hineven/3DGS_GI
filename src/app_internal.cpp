/*
 * Created: 2024/11/7
 * Author:  hineven
 * See LICENSE for licensing.
 */

#include <iostream>
#include "app_internal.h"
#include "renderer.h"

AppInternal::AppInternal() {}

AppInternal::~AppInternal() {}


static AppInternal * GAppInternalInstance;

int AppInternal::Run () {
    // Create window and gfx context
    window_ = gfxCreateWindow(GetWindowWidth(), GetWindowHeight());
    gfx_ = gfxCreateContext(
            window_
#ifndef NDEBUG
            ,
            kGfxCreateContextFlag_EnableShaderDebugging
            | kGfxCreateContextFlag_EnableDebugLayer
#endif
    );

//    auto dev = gfxGetDevice(gfx_);
//    if(dev == nullptr) {
//        puts("qwqeqwe");
//    }

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
        scene_.LoadGaussians(root_path + "data/counter/point_cloud/iteration_7000/point_cloud.ply");
        scene_.UpdateDeviceScene();
    }


    auto renderer = std::make_unique<Renderer>();

    renderer->Initialize();

    // Main loop
    while(!gfxWindowIsCloseRequested(window_)) {
        gfxWindowPumpEvents(window_);

        // Render
        renderer->Render();

        // Timed sections
        auto queries = renderer->CollectTimedSections();
        // TODO profile them in GUI

        gfxFrame(gfx_);
    }

    gfxFinish(gfx_);

    // Destroy renderer
    renderer->Destroy();
    renderer.reset();

    // Destroy scene
    scene_.DestroyDeviceScene();

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


/*
 * Created: 2024/11/7
 * Author:  hineven
 * See LICENSE for licensing.
 */

#include <iostream>
#include "app_internal.h"

AppInternal::AppInternal() {}

AppInternal::~AppInternal() {}

int AppInternal::Run() {
    // Create window and gfx context
    window_ = gfxCreateWindow(1280, 720);
    gfx_ = gfxCreateContext(
            window_
#ifndef NDEBUG
            ,
            kGfxCreateContextFlag_EnableShaderDebugging
            | kGfxCreateContextFlag_EnableDebugLayer
#endif
    );

    // Detect shader path
    std::string shader_path = "";
    bool found = false;
    for(int depth = 0; depth < 4; depth++) {
        if(std::filesystem::exists(shader_path + "src/device_shared.hlsl")) {
            found = true;
            break;
        }
        shader_path += "../";
    }
    if(!found) {
        std::cerr << "Cannot find shader path" << std::endl;
        return 1;
    }
    // Get the absolute path
    shader_path = std::filesystem::absolute(shader_path).string();
    std::cout << "Shader path: " << shader_path << std::endl;
    shader_path_ = shader_path;

    // Main loop
    while(!gfxWindowIsCloseRequested(window_)) {
        gfxWindowPumpEvents(window_);



        gfxFrame(gfx_);
    }

    return 0;
}
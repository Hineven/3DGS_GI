/*
 * Created: 2024/11/7
 * Author:  hineven
 * See LICENSE for licensing.
 */

#ifndef INC_3DGS_ADVGI_APP_INTERNAL_H
#define INC_3DGS_ADVGI_APP_INTERNAL_H

#include <filesystem>
#include "gfx_window.h"

class AppInternal {
public:
    AppInternal();
    ~AppInternal();
    int Run();

    inline GfxWindow& GetWindow() { return window_; }
    inline GfxContext& GetGfx() { return gfx_; }
    inline std::string GetShaderPath() { return shader_path_; }

protected:

    GfxWindow window_;
    GfxContext gfx_;

    // Base directory of all shader files.
    // Detected with src/device_shared.hlsl
    std::string shader_path_;
};

#endif //INC_3DGS_ADVGI_APP_INTERNAL_H

/*
 * Created: 2024/11/7
 * Author:  hineven
 * See LICENSE for licensing.
 */

#ifndef INC_3DGS_ADVGI_APP_INTERNAL_H
#define INC_3DGS_ADVGI_APP_INTERNAL_H

#include <filesystem>
#include "gfx_window.h"
#include "scene.h"

class AppInternal {
public:
    AppInternal();
    ~AppInternal();
    int Run ();

    inline GfxWindow& GetWindow() { return window_; }
    inline GfxContext& GetGfx() { return gfx_; }
    inline const std::string & GetRootPath() { return root_path_; }

    inline int GetWindowWidth() { return 1920; }
    inline int GetWindowHeight() { return 1088; }

    inline Scene & GetScene () { return scene_; }

    static AppInternal & GetInstance() ;

    friend class AppMain;

protected:

    GfxWindow window_;
    GfxContext gfx_;

    Scene scene_;

    // Base directory of project root. Used to locate shader and asset files.
    // Detected with src/device_shared.hlsl
    std::string root_path_;

    struct Samplers {
        GfxSamplerState linear_wrap;
        GfxSamplerState point_wrap;
        GfxSamplerState linear_clamp;
        GfxSamplerState point_clamp;
    } samplers_;

    float camera_speed_ {1.f};


    static void SetSingleton (AppInternal *singleton) ;

public:

    const Samplers & GetSamplers () const { return samplers_; }
};

#endif //INC_3DGS_ADVGI_APP_INTERNAL_H

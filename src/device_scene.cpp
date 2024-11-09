/*
 * Created: 2024/11/9
 * Author:  hineven
 * See LICENSE for licensing.
 */

#include "device_scene.h"
#include "app_internal.h"


DeviceScene::~DeviceScene () {
    Destroy();
}

void DeviceScene::Upload (const Scene & scene) {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    Destroy();
    gaussian_position = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * sizeof(glm::vec3), scene.gs_positions_.data()
    );
    gaussian_alpha = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * sizeof(float), scene.gs_alphas_.data()
    );
    gaussian_rotation = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * sizeof(glm::vec4), scene.gs_rotations_.data()
    );
    gaussian_scale = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * sizeof(glm::vec3), scene.gs_scales_.data()
    );
    gaussian_color = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * sizeof(glm::vec3), scene.gs_colors_.data()
    );
    gaussian_sh1 = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * 3 * sizeof(glm::vec3), scene.gs_sh1_.data()
    );
    gaussian_sh2 = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * 5 * sizeof(glm::vec3), scene.gs_sh2_.data()
    );
    gaussian_sh3 = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * 7 * sizeof(glm::vec3), scene.gs_sh3_.data()
    );
}

void DeviceScene::Destroy () {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    gfxDestroyBuffer(gfx, gaussian_position);
    gfxDestroyBuffer(gfx, gaussian_alpha);
    gfxDestroyBuffer(gfx, gaussian_rotation);
    gfxDestroyBuffer(gfx, gaussian_scale);
    gfxDestroyBuffer(gfx, gaussian_color);
    gfxDestroyBuffer(gfx, gaussian_sh1);
    gfxDestroyBuffer(gfx, gaussian_sh2);
    gfxDestroyBuffer(gfx, gaussian_sh3);
}

void DeviceScene::Bind(const GfxProgram &program) {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    gfxProgramSetParameter(gfx, program, "g_GaussianPositionBuffer", gaussian_position);
    gfxProgramSetParameter(gfx, program, "g_GaussianAlphaBuffer", gaussian_alpha);
    gfxProgramSetParameter(gfx, program, "g_GaussianRotationBuffer", gaussian_rotation);
    gfxProgramSetParameter(gfx, program, "g_GaussianScaleBuffer", gaussian_scale);
    gfxProgramSetParameter(gfx, program, "g_GaussianColorBuffer", gaussian_color);
    gfxProgramSetParameter(gfx, program, "g_GaussianSH1Buffer", gaussian_sh1);
    gfxProgramSetParameter(gfx, program, "g_GaussianSH2Buffer", gaussian_sh2);
    gfxProgramSetParameter(gfx, program, "g_GaussianSH3Buffer", gaussian_sh3);
}

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
    auto &gfx = AppInternal::GetInstance().GetGfx();
    Destroy();

    std::cout << "Uploading scene to device" << std::endl;
    gaussian_position = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * sizeof(glm::vec3), scene.gs_positions_.data()
    );
    gaussian_position.setName("GaussianPositionBuffer");
    gaussian_alpha = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * sizeof(float), scene.gs_alphas_.data()
    );
    gaussian_alpha.setName("GaussianAlphaBuffer");
    gaussian_rotation = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * sizeof(glm::vec4), scene.gs_rotations_.data()
    );
    gaussian_rotation.setName("GaussianRotationBuffer");
    gaussian_scale = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * sizeof(glm::vec3), scene.gs_scales_.data()
    );
    gaussian_scale.setName("GaussianScaleBuffer");
    gaussian_color = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * sizeof(glm::vec3), scene.gs_colors_.data()
    );
    gaussian_color.setName("GaussianColorBuffer");
    gaussian_sh1 = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * 3 * sizeof(glm::vec3), scene.gs_sh1_.data()
    );
    gaussian_sh1.setName("GaussianSH1Buffer");
    gaussian_sh2 = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * 5 * sizeof(glm::vec3), scene.gs_sh2_.data()
    );
    gaussian_sh2.setName("GaussianSH2Buffer");
    gaussian_sh3 = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * 7 * sizeof(glm::vec3), scene.gs_sh3_.data()
    );
    gaussian_sh3.setName("GaussianSH3Buffer");

    gsi_instance_base_ = gfxCreateBuffer(gfx, scene.num_instances_ * sizeof(uint32_t), scene.gsi_gs_index_offsets_.data());
    gsi_instance_base_.setName("GSInstanceBaseBuffer");
    gsi_instance_count_ = gfxCreateBuffer(gfx, scene.num_instances_ * sizeof(uint32_t), scene.gsi_gs_counts_.data());
    gsi_instance_count_.setName("GSInstanceCountBuffer");
    gsi_transform_ = gfxCreateBuffer(gfx, scene.num_instances_ * sizeof(glm::mat4x3), scene.gsi_transforms_.data());
    gsi_transform_.setName("GSITransformBuffer");
    gsi_inv_transform_ = gfxCreateBuffer(gfx, scene.num_instances_ * sizeof(glm::mat4x3), scene.gsi_inv_transforms_.data());
    gsi_inv_transform_.setName("GSIInvTransformBuffer");
    gsi_normal_transform_ = gfxCreateBuffer(gfx, scene.num_instances_ * sizeof(glm::mat3x3), scene.gsi_normal_transforms_.data());
    gsi_normal_transform_.setName("GSINormalTransformBuffer");

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
    gfxDestroyBuffer(gfx, gsi_instance_base_);
    gfxDestroyBuffer(gfx, gsi_instance_count_);
    gfxDestroyBuffer(gfx, gsi_transform_);
    gfxDestroyBuffer(gfx, gsi_inv_transform_);
    gfxDestroyBuffer(gfx, gsi_normal_transform_);
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

    gfxProgramSetParameter(gfx, program, "g_InstanceGaussianIndexOffsetBuffer", gsi_instance_base_);
    gfxProgramSetParameter(gfx, program, "g_InstanceGaussianCountBuffer", gsi_instance_count_);
    gfxProgramSetParameter(gfx, program, "g_InstanceTransformBuffer", gsi_transform_);
    gfxProgramSetParameter(gfx, program, "g_InstanceInvTransformBuffer", gsi_inv_transform_);
    gfxProgramSetParameter(gfx, program, "g_InstanceNormalTransformBuffer", gsi_normal_transform_);
}

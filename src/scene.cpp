/*
 * Created: 2024/11/7
 * Author:  hineven
 * See LICENSE for licensing.
 */
#include <happly.h>
#include "scene.h"
#include "device_scene.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "app_internal.h"

Scene::Scene () {
    device_scene_ = std::make_unique<DeviceScene>();
}

bool Scene::LoadGaussians (std::filesystem::path path) {
    if(path.extension() != ".ply") {
        app_warning("only .ply files are supported.");
        return false;
    }
    happly::PLYData plyIn(path.string());
    auto & element = plyIn.getElement("vertex");
    // Positions
    {
        auto x = element.getProperty<float>("x");
        auto y = element.getProperty<float>("y");
        auto z = element.getProperty<float>("z");
        num_gaussians_ = (int)x.size();
        gs_positions_.resize(num_gaussians_);
        for(int i = 0; i < x.size(); i++) {
            gs_positions_[i] = glm::vec3(x[i], y[i], z[i]);
        }
    }
    // Colors
    {
        auto r = element.getProperty<float>("f_dc_0");
        auto g = element.getProperty<float>("f_dc_1");
        auto b = element.getProperty<float>("f_dc_2");
        gs_colors_.resize(num_gaussians_);
        for(int i = 0; i < r.size(); i++) {
            gs_colors_[i] = glm::vec3(r[i], g[i], b[i]);
        }
    }
    // SH
    {
        gs_sh1_.resize(num_gaussians_ * 3);
        gs_sh2_.resize(num_gaussians_ * 5);
        gs_sh3_.resize(num_gaussians_ * 7);
        std::reference_wrapper<std::vector<glm::vec3>> arr[] = {gs_sh1_, gs_sh2_, gs_sh3_};
        int coeff_top = 0;
        for(int degree = 1; degree <= 3; degree ++) {
            int num_coeff = degree * 2 + 1;
            for(int coeff = 0; coeff < num_coeff; coeff ++) {
                for (int ch = 0; ch < 3; ch++) {
                    std::string name = "f_rest_" + std::to_string(coeff_top);
                    auto data = element.getProperty<float>(name);
                    for(int i = 0; i < data.size(); i++) {
                        auto & v = arr[degree - 1];
                        v.get()[i * num_coeff + coeff][ch] = data[i];
                    }
                    coeff_top++;
                }
            }
        }
    }
    // Alphas
    {
        auto alphas = element.getProperty<float>("opacity");
        gs_alphas_.resize(num_gaussians_);
        for(int i = 0; i < alphas.size(); i++) {
            // activation: sigmoid
            alphas[i] = 1.f / (1.f + exp(-alphas[i]));
            gs_alphas_[i] = alphas[i];
        }
    }
    // Scales
    {
        auto scale_x = element.getProperty<float>("scale_0");
        auto scale_y = element.getProperty<float>("scale_1");
        auto scale_z = element.getProperty<float>("scale_2");
        // Activation: exp
        for(int i = 0; i < scale_x.size(); i++) {
            scale_x[i] = exp(scale_x[i]);
            scale_y[i] = exp(scale_y[i]);
            scale_z[i] = exp(scale_z[i]);
        }
        gs_scales_.resize(num_gaussians_);
        for(int i = 0; i < scale_x.size(); i++) {
            gs_scales_[i] = glm::vec3(scale_x[i], scale_y[i], scale_z[i]);
        }
    }
    // Rotations
    {
        // W first!
        auto rotation_w = element.getProperty<float>("rot_0");
        auto rotation_x = element.getProperty<float>("rot_1");
        auto rotation_y = element.getProperty<float>("rot_2");
        auto rotation_z = element.getProperty<float>("rot_3");
        gs_rotations_.resize(num_gaussians_);
        for(int i = 0; i < rotation_x.size(); i++) {
            gs_rotations_[i] = glm::vec4(rotation_x[i], rotation_y[i], rotation_z[i], rotation_w[i]);
            // activation: normalize
            auto & q = gs_rotations_[i];
            float len = sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
            q /= len;
        }
    }

    // FIXME
    // Limit the number to 10 for now
    num_gaussians_ = 10;

    // We assume that there're only 1 instance for now
    num_instances_ = 1;
    gsi_transforms_.resize(1);
    gsi_transforms_[0] = glm::mat4x3(1.f);

    gsi_inv_transforms_.resize(1);
    gsi_inv_transforms_[0] = glm::mat4x3(1.f);

    gsi_normal_transforms_.resize(1);
    gsi_normal_transforms_[0] = glm::mat3x3(1.f);

    gsi_gs_index_offsets_.resize(1);
    gsi_gs_index_offsets_[0] = 0;

    gsi_gs_counts_.resize(1);
    gsi_gs_counts_[0] = num_gaussians_;

    return true;
}

void Scene::UpdateDeviceScene() {
    Scene & scene = *this;
    if(!device_scene_) {
        device_scene_ = std::make_unique<DeviceScene>();
    }
    device_scene_->Upload(scene);
}

Scene::~Scene () {
    // make unique_ptr work
}

void Scene::DestroyDeviceScene() {
    device_scene_.reset();
}

glm::mat4x4 Camera::GetViewMatrix () const {
    auto view_matrix = glm::lookAt(position, position + direction, up);
    return view_matrix;
}

glm::mat4x4 Camera::GetProjectionMatrix () const {
#undef near
#undef far
    auto width = AppInternal::GetInstance().GetWindowWidth();
    auto height = AppInternal::GetInstance().GetWindowHeight();
    auto aspect = (float)width / height;
    auto projection_matrix = glm::perspective(fov_y, aspect, near, far);
    return projection_matrix;
}
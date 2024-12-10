/*
 * Created: 2024/11/7
 * Author:  hineven
 * See LICENSE for licensing.
 */
#include <map>
#include <happly.h>
#include <gfx.h>
#include <gfx_scene.h>
#include "scene.h"
#include "device_scene.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "app_internal.h"

Scene::Scene () {}

bool Scene::LoadEnvironmentMap(std::filesystem::path path) {
    if (path.extension() != ".hdr" && path.extension() != ".exr") {
        app_warning("Only .hdr and .exr files are supported. Current: " << path.extension());
        return false;
    }
    if (!std::filesystem::exists(path)) {
        app_warning("File does not exist.");
        return false;
    }
    environment_map_path_ = path;
    return true;
}


bool Scene::LoadGaussians (std::filesystem::path path, bool always_load_sh) {
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
    bool load_sh = true;
    if (element.hasProperty("base_color_0")) {
        load_sh = false;

        // Try to load attributes from the format specified by paper from Nanking Univ
        // [ECCV2024] Relightable 3D Gaussian: Real-time Point Cloud Relighting with BRDF Decomposition and Ray Tracing

        // Albedo
        {
            auto r = element.getProperty<float>("base_color_0");
            auto g = element.getProperty<float>("base_color_1");
            auto b = element.getProperty<float>("base_color_2");
            gs_albedos_.resize(num_gaussians_);
            for (int i = 0; i < r.size(); i++) {
                auto raw = glm::vec3(r[i], g[i], b[i]);
                // activation: scaled sigmoid
                gs_albedos_[i] = 0.03f + 0.77f / (1.f + exp(-raw));
            }
        }
        // Roughness
        {
            auto r = element.getProperty<float>("roughness");
            gs_roughnesses_.resize(num_gaussians_);
            for (int i = 0; i < r.size(); i++) {
                // activation: sigmoid
                gs_roughnesses_[i] = 0.09f + 0.9f / (1.f + exp(-r[i]));
            }
        }

        // No lambertian term involved in their bsdf model. So they are always 'metals' (that is, metallic == 1).
    }
    if (load_sh || always_load_sh) {
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
    // Normals
    {
        auto nx = element.getProperty<float>("nx");
        auto ny = element.getProperty<float>("ny");
        auto nz = element.getProperty<float>("nz");
        gs_normals_.resize(num_gaussians_);
        for(int i = 0; i < nx.size(); i++) {
            // activation: normalize
            gs_normals_[i] = normalize(glm::vec3(nx[i], ny[i], nz[i]));
        }
    }

    // Limit the number to 10 for now
    // num_gaussians_ = 50;

    // We assume that there is only 1 instance for now
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

    UpdateBounds ();

    // Manually insert lights
    // only 1 directional light for now.
    lights_.resize(1);
    lights_[0] = Light {
        LIGHT_TYPE_DIRECTIONAL,
        1.f, {}, {},
        glm::vec3(0.f, 0.f, 1.f),
    };
    light_data_.resize(lights_.size() * 4);
    light_data_[0] = {0.f, 0.f, 1.f};
    light_data_[1] = {0.f, 0.f, 0.f};
    light_data_[2] = {0.f, 0.f, 0.f};
    light_data_[3] = {3.f, 2.2f, 2.f};

    return true;
}

struct Bounds {
    glm::vec3 mn;
    glm::vec3 mx;
};

void Scene::UpdateBounds() {
    std::map<std::pair<int, int> , Bounds> cached_bounds;
    float scale_multiplier = 3.f;
    gsi_bounds_min.clear();
    gsi_bounds_max.clear();
    bounds_min_ = glm::vec3(FLT_MAX);
    bounds_max_ = glm::vec3(-FLT_MAX);
    for (int i = 0; i < (int)gsi_transforms_.size(); i++) {
        auto key = std::make_pair(gsi_gs_index_offsets_[i], gsi_gs_counts_[i]);
        auto it = cached_bounds.find(key);
        Bounds bounds;
        if (it != cached_bounds.end()) {
            bounds = it->second;
        } else {
            bounds.mn = glm::vec3(FLT_MAX);
            bounds.mx = glm::vec3(-FLT_MAX);
            for (int j = gsi_gs_index_offsets_[i]; j < gsi_gs_index_offsets_[i] + gsi_gs_counts_[i]; j++) {
                auto & pos = gs_positions_[j];
                auto & scale = gs_scales_[j];
                auto mn = pos - scale * scale_multiplier;
                auto mx = pos + scale * scale_multiplier;
                bounds.mn = glm::min(bounds.mn, pos);
                bounds.mx = glm::max(bounds.mx, pos);
            }
            cached_bounds[key] = bounds;
        }
        gsi_bounds_min.push_back(bounds.mn);
        gsi_bounds_max.push_back(bounds.mx);
        glm::vec3 points[8];
        points[0] = bounds.mn;
        points[1] = glm::vec3(bounds.mn.x, bounds.mn.y, bounds.mx.z);
        points[2] = glm::vec3(bounds.mn.x, bounds.mx.y, bounds.mn.z);
        points[3] = glm::vec3(bounds.mn.x, bounds.mx.y, bounds.mx.z);
        points[4] = glm::vec3(bounds.mx.x, bounds.mn.y, bounds.mn.z);
        points[5] = glm::vec3(bounds.mx.x, bounds.mn.y, bounds.mx.z);
        points[6] = glm::vec3(bounds.mx.x, bounds.mx.y, bounds.mn.z);
        points[7] = bounds.mx;
        // Transform to world space and update scene bounds
        for (int j = 0; j < 8; j++) {
            auto p = gsi_transforms_[i] * glm::vec4(points[j], 1.f);
            bounds_min_ = glm::min(bounds_min_, p);
            bounds_max_ = glm::max(bounds_max_, p);
        }
    }
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
    auto projection_matrix = glm::perspectiveRH_ZO(fov_y, aspect, near, far);
    return projection_matrix;
}

CameraDescription Camera::PackDescription(
    int film_width, int film_height,
    const CameraDescription & prev_description
) const {
    auto aspect = (double)film_width / film_height;
    auto tan_fov_y = tan(fov_y / 2.0);
    auto two_tan_fov_y = float(tan_fov_y * 2.0);

    glm::vec3 axis_forward = direction;
    // Camera forward is -z axis
    glm::vec3 axis_right = glm::normalize(glm::cross(axis_forward, up));
    glm::vec3 axis_up = glm::normalize(glm::cross(axis_right, axis_forward));
    // Thus, normalize(axis_forward + axis_right * ndc.x + axis_up * ndc.y) is the camera ray direction
    axis_up    *= tan_fov_y;
    axis_right *= tan_fov_y * aspect;

    CameraDescription ret = {};

    ret.View = GetViewMatrix();
    ret.Projection = GetProjectionMatrix();
    ret.ProjectionView = ret.Projection * ret.View;
    ret.Reprojection = glm::mat4(
        glm::dmat4(prev_description.ProjectionView) * glm::inverse(glm::dmat4(ret.ProjectionView))
    );

    ret.Position = position;
    ret.NearPlane = near;

    ret.Direction = axis_forward;
    ret.FarPlane  = far;

    ret.Focal = glm::vec2(film_width, film_height) / two_tan_fov_y;
    ret.FieldOfView = glm::vec2(aspect * two_tan_fov_y, two_tan_fov_y);

    ret.FilmDimensions = glm::ivec2(film_width, film_height);
    ret.InvFilmDimensions = glm::vec2(1.0 / film_width, 1.0 / film_height);

    ret.Right = axis_right;
    ret.Flags = 0;

    ret.Up = axis_up;
    ret.Padding0 = 0;

    ret.FilmTexelSize = 1.f / glm::vec2(film_width, film_height);
    ret.HZBBaseTexelSize = 2.f * ret.FilmTexelSize;

    return ret;
}

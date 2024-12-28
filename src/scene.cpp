/*
 * Created: 2024/11/7
 * Author:  hineven
 * See LICENSE for licensing.
 */
#include <map>
#include <happly.h>
#include <gfx.h>
#include <gfx_scene.h>
#include "3dgs_shared_lib.hlsl"
#include "scene.h"
#include "device_scene.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "app_internal.h"

Scene::Scene () {
    InitializeLights();
}

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

int Scene::LoadGaussians (std::filesystem::path path, bool always_load_sh) {
    if(path.extension() != ".ply") {
        app_warning("only .ply files are supported.");
        return -1;
    }

    happly::PLYData plyIn(path.string());
    auto & element = plyIn.getElement("vertex");
    // We reserved 24 bits for the count of gaussians for each instance.
    assert(element.count < (1 << 24));
    int old_num_gaussians = num_gaussians_;
    int inst_num_gaussians = element.count;
    num_gaussians_ += inst_num_gaussians;
    // Positions
    {
        auto x = element.getProperty<float>("x");
        auto y = element.getProperty<float>("y");
        auto z = element.getProperty<float>("z");
        gs_positions_.resize(num_gaussians_);
        for(int i = 0; i < x.size(); i++) {
            gs_positions_[old_num_gaussians + i] = glm::vec3(x[i], y[i], z[i]);
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
                gs_albedos_[old_num_gaussians + i] = 0.03f + 0.77f / (1.f + exp(-raw));
            }
        }
        // Roughness
        {
            auto r = element.getProperty<float>("roughness");
            gs_roughnesses_.resize(num_gaussians_);
            for (int i = 0; i < r.size(); i++) {
                // activation: sigmoid
                gs_roughnesses_[old_num_gaussians + i] = 0.09f + 0.9f / (1.f + exp(-r[i]));
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
                gs_colors_[old_num_gaussians + i] = glm::vec3(r[i], g[i], b[i]);
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
                            v.get()[(old_num_gaussians + i) * num_coeff + coeff][ch] = data[i];
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
            gs_alphas_[old_num_gaussians + i] = alphas[i];
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
            gs_scales_[old_num_gaussians + i] = glm::vec3(scale_x[i], scale_y[i], scale_z[i]);
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
            gs_rotations_[old_num_gaussians + i] = glm::vec4(rotation_x[i], rotation_y[i], rotation_z[i], rotation_w[i]);
            // activation: normalize
            auto & q = gs_rotations_[old_num_gaussians + i];
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
            gs_normals_[old_num_gaussians + i] = normalize(glm::vec3(nx[i], ny[i], nz[i]));
        }
    }

    num_instances_ ++;
    gsi_transforms_.resize(num_instances_);
    gsi_inv_transforms_.resize(num_instances_);
    gsi_normal_transforms_.resize(num_instances_);
    gsi_inv_normal_transforms_.resize(num_instances_);

    gsi_bounds_min.resize(num_instances_);
    gsi_bounds_max.resize(num_instances_);

    int curr_instance = num_instances_ - 1;
    gsi_types_.resize(num_instances_);
    gsi_types_[curr_instance] = InstanceType::eGaussians;
    gsi_gs_index_offsets_.resize(num_instances_);
    gsi_gs_index_offsets_[curr_instance] = old_num_gaussians;
    gsi_gs_mesh_index_offsets_.resize(num_instances_);
    gsi_gs_mesh_index_offsets_[curr_instance] = 0;
    gsi_mesh_num_indices_.resize(num_instances_);
    gsi_mesh_num_indices_[curr_instance] = 0;
    gsi_gs_counts_.resize(num_instances_);
    gsi_gs_counts_[curr_instance] = inst_num_gaussians;

    SetInstanceTransform(curr_instance, glm::mat4x3(1.f));

    UpdateBoundsForInstance(curr_instance);

    UpdateSceneBounds ();

    return curr_instance;
}

int Scene::DuplicateInstance (int src_instance) {
    num_instances_ ++;
    int curr_instance = num_instances_ - 1;
    gsi_types_.resize(num_instances_);
    gsi_transforms_.resize(num_instances_);
    gsi_inv_transforms_.resize(num_instances_);
    gsi_normal_transforms_.resize(num_instances_);
    gsi_inv_normal_transforms_.resize(num_instances_);
    gsi_bounds_min.resize(num_instances_);
    gsi_bounds_max.resize(num_instances_);
    gsi_gs_index_offsets_.resize(num_instances_);
    gsi_gs_mesh_index_offsets_.resize(num_instances_);
    gsi_mesh_num_indices_.resize(num_instances_);
    gsi_gs_counts_.resize(num_instances_);

    // Copy data
    gsi_types_[curr_instance] = gsi_types_[src_instance];
    gsi_gs_index_offsets_[curr_instance] = gsi_gs_index_offsets_[src_instance];
    gsi_gs_mesh_index_offsets_[curr_instance] = gsi_gs_mesh_index_offsets_[src_instance];
    gsi_mesh_num_indices_[curr_instance] = gsi_mesh_num_indices_[src_instance];
    gsi_gs_counts_[curr_instance] = gsi_gs_counts_[src_instance];
    gsi_bounds_min[curr_instance] = gsi_bounds_min[src_instance];
    gsi_bounds_max[curr_instance] = gsi_bounds_max[src_instance];

    SetInstanceTransform(curr_instance, glm::mat4x3(1.f));
    // UpdateBoundsForInstance(curr_instance);
    UpdateSceneBounds ();

    return curr_instance;
}



void Scene::SetInstanceTransform(int instance, glm::mat4x3 to_world_transform) {
    gsi_transforms_[instance] = to_world_transform;
    {
        glm::mat3 inner = gsi_transforms_[instance];
        glm::mat3 inv_inner = glm::inverse(inner);
        gsi_inv_transforms_[instance] = glm::mat4x3(inv_inner);
        gsi_inv_transforms_[instance][3][0] = -gsi_transforms_[instance][3][0];
        gsi_inv_transforms_[instance][3][1] = -gsi_transforms_[instance][3][1];
        gsi_inv_transforms_[instance][3][2] = -gsi_transforms_[instance][3][2];
    }

    gsi_normal_transforms_[instance] = glm::transpose(glm::inverse(glm::mat3x3(to_world_transform)));
    gsi_inv_normal_transforms_[instance] = glm::inverse(gsi_normal_transforms_[instance]);
}


void Scene::SetLight (LightType type, const LightData & LD, int index) {
    if (type == LightType::eDirectional && index) {
        app_warning("Directional light is always the first light.");
        index = 0;
    }
    if (type == LightType::eSky) {
        if (index != 1) {
            if (index) app_warning("Sky light is always the second light.");
            index = 1;
        }
    }
    if (type == LightType::eArea && index <= 1) {
        app_warning("The first two lights are reserved for directional and sky lights.");
        return ;
    }
    light_data_[index] = LD;
}

void Scene::SetDirectionalLight(const LightData &light) {
    SetLight(LightType::eDirectional, light, 0);
}

void Scene::SetSkyLight(const LightData &light) {
    SetLight(LightType::eSky, light, 1);
}

void Scene::SetAreaLight(int area_light_index, const LightData &light) {
    SetLight(LightType::eArea, light, area_light_index + 2);
}

LightData Scene::GetDirectionalLight() {
    return light_data_[0];
}

LightData Scene::GetSkyLight() {
    return light_data_[1];
}

void Scene::InitializeLights () {
    // Manually insert lights
    // only 1 directional light & 1 sky light for now.
    glm::vec3 di_radiance = {3.f, 2.3f, 2.f};

    light_data_.resize(2);
    light_data_[0].Radiance = di_radiance;
    light_data_[0].V1 = {0.f, 0.f, 1.f};
    // Skylight data is not used for now, no initialization required
}

struct Bounds {
    glm::vec3 mn;
    glm::vec3 mx;
};

void Scene::UpdateBoundsForInstance (int instance) {
    if (gsi_types_[instance] == InstanceType::eGaussians) {
        float scale_multiplier = 3.f;
        Bounds bounds;

        bounds.mn = glm::vec3(FLT_MAX);
        bounds.mx = glm::vec3(-FLT_MAX);
        for (int j = gsi_gs_index_offsets_[instance]; j < gsi_gs_index_offsets_[instance] + gsi_gs_counts_[instance]; j++) {
            auto & pos = gs_positions_[j];
            auto & scale = gs_scales_[j];
            // TODO consider rotation
            float mscale = glm::max(scale.x, glm::max(scale.y, scale.z));
            auto mn = pos - mscale * scale_multiplier;
            auto mx = pos + mscale * scale_multiplier;
            bounds.mn = glm::min(bounds.mn, mn);
            bounds.mx = glm::max(bounds.mx, mx);
        }
        gsi_bounds_min[instance] = bounds.mn;
        gsi_bounds_max[instance] = bounds.mx;
    }
}

void Scene::UpdateSceneBounds () {
    scene_bounds_min_ = glm::vec3(FLT_MAX);
    scene_bounds_max_ = glm::vec3(-FLT_MAX);
    for (int i = 0; i < num_instances_; i++) {
        auto bounds = AABB{
            gsi_bounds_min[i],
            gsi_bounds_max[i]
        };
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
            scene_bounds_min_ = glm::min(scene_bounds_min_, p);
            scene_bounds_max_ = glm::max(scene_bounds_max_, p);
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

void Scene::UpdateDeviceLights() {
    Scene & scene = *this;
    if(!device_scene_) {
        device_scene_ = std::make_unique<DeviceScene>();
    }
    device_scene_->UpdateLights(scene);
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
    glm::mat4 projection_matrix;
    if (type == CameraType::ePerspective)
        projection_matrix = glm::perspectiveRH_ZO(fov_y, aspect, near, far);
    else if (type == CameraType::eOrthographic)
        projection_matrix = glm::orthoRH_ZO(min_clip.x, max_clip.x, min_clip.y, max_clip.y, near, far);
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
    ret.Flags = ((uint)type) & 0xfu;

    ret.Up = axis_up;
    ret.Padding = 0;

    ret.FilmTexelSize = 1.f / glm::vec2(film_width, film_height);
    ret.HZBBaseTexelSize = 2.f * ret.FilmTexelSize;

    return ret;
}

/*
 * Created: 2024/11/7
 * Author:  hineven
 * See LICENSE for licensing.
 */

#ifndef INC_3DGS_ADVGI_SCENE_H
#define INC_3DGS_ADVGI_SCENE_H

#include <vector>
#include <filesystem>
#include <glm/glm.hpp>
#include "common.h"
#include "3dgs_shared.hlsl"

class DeviceScene;


enum class LightType {
    eDirectional = LIGHT_TYPE_DIRECTIONAL,
    eSky = LIGHT_TYPE_SKY,
    eArea = LIGHT_TYPE_AREA
};

struct AABB {
    glm::vec3 mn;
    glm::vec3 mx;
};

enum class CameraType {
    ePerspective = CAMERA_TYPE_PERSPECTIVE,
    eOrthographic = CAMERA_TYPE_ORTHOGRAPHIC
};


class Camera {
public:
    Camera() = default;
    ~Camera() = default;

    CameraDescription PackDescription (
        int film_width, int film_height,
        const CameraDescription & prev_description
    ) const;

    glm::mat4x4 GetViewMatrix () const;
    glm::mat4x4 GetProjectionMatrix () const;

    glm::vec3 position {0, 0, 0};
    glm::vec3 direction {0, 0, -1};
    glm::vec3 up {0, 1, 0};
    glm::vec2 min_clip {-1, -1};
    glm::vec2 max_clip {1, 1};
    // radians
    float fov_y {1.f};

    CameraType type {CameraType::ePerspective};

    // fk windows
#undef near
#undef far
    float near  {0.05f};
    float far   {50.f};
};

class Scene {
public:
    Scene();

    // Load a hdr environment map
    bool LoadEnvironmentMap (std::filesystem::path);

    // Clear and load gaussians from some .ply file.
    // If the gaussians are PBR ready, we won't load the SH coeffs and colors.
    bool LoadGaussians (std::filesystem::path path, bool always_load_sh = false) ;

    // Synchronize the data on host to device.
    void UpdateDeviceScene () ;
    void DestroyDeviceScene () ;

    void UpdateDeviceLights ();

    void SetLight (LightType type, const LightData & LD, int index = 0) ;

    LightData GetDirectionalLight ();
    LightData GetSkyLight ();

    void SetDirectionalLight (const LightData & light);
    void SetSkyLight (const LightData & light);
    void SetAreaLight (int area_light_index, const LightData & light);

    LightData GetLight (int index) ;

    inline Camera & GetCamera() { return camera_; }

    inline DeviceScene & GetDeviceScene() { return *device_scene_; }

    inline int GetNumGaussians () const { return num_gaussians_; }

    inline int GetNumInstances () const { return num_instances_; }

    inline glm::vec3 GetBoundsMin () const { return bounds_min_; }

    inline glm::vec3 GetBoundsMax () const { return bounds_max_; }

    inline int GetNumLights () const { return light_data_.size(); }

    inline int GetInstanceNumGaussians (int instance_index) const {
        return gsi_gs_counts_[instance_index];
    }

    inline void SetNumLights (int num_lights) {
        assert(num_lights >= 2);
        light_data_.resize(num_lights);
    }

    inline AABB GetInstanceAABB (int id) {
        return {gsi_bounds_min[id], gsi_bounds_max[id]};
    }

    // Local to world transform
    inline glm::mat4x3 GetInstanceTransform (int id) {
        return gsi_transforms_[id];
    }

    // Local to world transform
    inline glm::mat3x3 GetInstanceTransformNormal (int id) {
        return gsi_normal_transforms_[id];
    }

    ~Scene();

    friend class DeviceScene;
    friend class Renderer;
protected:

    void InitializeLights ();

    // Update the bounds of all instances and the entire scene
    void UpdateBounds ();

    Camera camera_ {};

    glm::vec3 bounds_min_;
    glm::vec3 bounds_max_;

    int num_gaussians_;
    std::vector<glm::vec3> gs_positions_;
    std::vector<glm::vec3> gs_colors_;
    std::vector<float>     gs_alphas_;
    std::vector<glm::vec3> gs_sh1_;
    std::vector<glm::vec3> gs_sh2_;
    std::vector<glm::vec3> gs_sh3_;
    // 3DGS datasets use 4-word quaternions directly for rotation
    std::vector<glm::vec4> gs_rotations_;
    std::vector<glm::vec3> gs_scales_;
    std::vector<glm::vec3> gs_normals_;

    std::vector<glm::vec3> gs_albedos_;
    std::vector<float>     gs_roughnesses_;

    int num_instances_;
    // GS instances
    std::vector<glm::vec3> gsi_bounds_min; // instance local bounds
    std::vector<glm::vec3> gsi_bounds_max; // instance local bounds
    std::vector<glm::mat4x3> gsi_transforms_;
    std::vector<glm::mat4x3> gsi_inv_transforms_;
    std::vector<glm::mat3> gsi_normal_transforms_;
    std::vector<glm::mat3> gsi_inv_normal_transforms_;
    // GS instance GS index offsets
    std::vector<int> gsi_gs_index_offsets_;
    // GS instances GS count
    std::vector<int> gsi_gs_counts_;

    std::unique_ptr<DeviceScene> device_scene_;

    // Lights (packed)
    // std::vector<Light> lights_;
    std::vector<LightData> light_data_;

    std::filesystem::path environment_map_path_;
};

#endif //INC_3DGS_ADVGI_SCENE_H

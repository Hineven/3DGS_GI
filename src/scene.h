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
    float fov_y {0.6f};

    CameraType type {CameraType::ePerspective};

    // fk windows
#undef near
#undef far
    float near  {0.05f};
    float far   {50.f};
};

enum class InstanceType {
    eGaussians,
    eMesh
};

struct InstanceTransform {
    glm::vec3 position {};
    glm::vec3 rotation {};
    glm::vec3 scale {1.f};
};

struct SceneInstance {
    InstanceType type;
    // Number of vertices or number of gaussians
    int num_vertices;
    int num_indices;
    // Index into the scene's gs_ / vertex_ arrays
    int vertex_offset;
    // index into the scene's instance arrays (only makes sense for meshes)
    int index_offset;
    AABB local_bounds;
    glm::mat4x3 to_world_transform;
    glm::mat4x3 to_local_transform;
    glm::mat3x3 to_world_normal_transform;
    glm::mat3x3 to_local_normal_transform;
};

class Scene {
public:
    Scene();

    // Load a hdr environment map
    bool LoadEnvironmentMap (std::filesystem::path);

    // Clear and load gaussians from some .ply file.
    // If the gaussians are PBR ready, we won't load the SH coeffs and colors (by default)
    // returns -1 if failed, otherwise the instance index of the loaded gaussians
    int LoadGaussians (std::filesystem::path path, bool always_load_sh = false) ;

    // Returns the starting index of loaded instances
    int LoadGltf (std::filesystem::path path);

    int DuplicateInstance (int src_instance);

    // Synchronize the data on host to device.
    void UpdateDeviceScene () ;
    void DestroyDeviceScene () ;

    void UpdateDeviceLights ();
    void UpdateDeviceTransforms ();
    void UpdateDeviceMaterials ();

    void SetLight (LightType type, const LightData & LD, int index = 0, int instance_index = -1) ;

    void OverwriteGaussianAlbedo (int instance_id, glm::vec3 albedo) ;
    void OverwriteGaussianRoughness (int instance_id, float roughness) ;

    LightData GetDirectionalLight ();
    LightData GetSkyLight ();

    void SetDirectionalLight (const LightData & light);
    void SetSkyLight (const LightData & light);
    void SetAreaLight (int area_light_index, const LightData & light, int instance_index = -1);

    inline SimpleMaterial GetInstanceMaterial(int index) {
        return gsi_materials_[index];
    }

    inline bool IsGaussianInstance (int index) {
        return gsi_types_[index] == InstanceType::eGaussians;
    }

    LightData GetLight (int index) ;

    inline Camera & GetCamera() { return camera_; }

    inline DeviceScene & GetDeviceScene() { return *device_scene_; }

    inline int GetNumGaussians () const { return num_gaussians_; }

    inline int GetNumInstances () const { return num_instances_; }

    inline glm::vec3 GetBoundsMin () const { return scene_bounds_min_; }

    inline glm::vec3 GetBoundsMax () const { return scene_bounds_max_; }

    inline int GetNumLights () const { return light_data_.size(); }

    inline int GetInstanceNumGaussians (int instance_index) const {
        return gsi_gs_counts_[instance_index];
    }

    inline void SetNumLights (int num_lights) {
        assert(num_lights >= 2);
        light_data_.resize(num_lights);
        light_instance_.resize(num_lights, -1);
    }

    inline AABB GetInstanceAABB (int id) {
        return {gsi_bounds_min[id], gsi_bounds_max[id]};
    }

    // Local to world transform
    inline glm::mat3x3 GetInstanceTransformNormal (int id) {
        return gsi_normal_transforms_[id];
    }

    inline SceneInstance GetInstance (int index) {
        return SceneInstance {
            .type = gsi_types_[index],
            .num_vertices = gsi_gs_counts_[index],
            .num_indices = gsi_mesh_num_indices_[index],
            .vertex_offset = gsi_gs_index_offsets_[index],
            .index_offset = gsi_mesh_index_offsets_[index],
            .local_bounds = {gsi_bounds_min[index], gsi_bounds_max[index]},
            .to_world_transform = gsi_transforms_[index],
            .to_local_transform = gsi_inv_transforms_[index],
            .to_world_normal_transform = gsi_normal_transforms_[index],
            .to_local_normal_transform = gsi_inv_normal_transforms_[index]
        };
    }

    void UpdateBoundsForInstance (int instance);

    void SetInstanceTransform (int instance, InstanceTransform transform);
    inline InstanceTransform GetInstanceTransform (int instance) {
        return {gsi_positions_[instance], gsi_rotations_[instance], gsi_scales_[instance]};
    }

    inline void SetInstanceMaterial (int instance, SimpleMaterial M) {
        gsi_materials_[instance] = M;
    }

    void SetInstanceActive (int instance, bool active) {
        gsi_active_[instance] = active;
    }

    void UpdateSceneBounds ();

    ~Scene();

    friend class DeviceScene;
    friend class Renderer;
protected:

    void SetInstanceTransformMatrix (int instance, glm::mat4x3 to_world_transform) ;
    // Local to world transform
    inline glm::mat4x3 GetInstanceTransformMatrix (int id) const {
        return gsi_transforms_[id];
    }
    void InitializeLights();

    Camera camera_ {};

    glm::vec3 scene_bounds_min_ {};
    glm::vec3 scene_bounds_max_ {};

    int num_gaussians_ {};
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

    int num_instances_ {};
    // instances (gs / mesn)
    std::vector<InstanceType> gsi_types_;
    std::vector<glm::vec3> gsi_bounds_min; // instance local bounds
    std::vector<glm::vec3> gsi_bounds_max; // instance local bounds
    std::vector<glm::mat4x3> gsi_transforms_;
    std::vector<glm::mat4x3> gsi_inv_transforms_;
    std::vector<glm::mat3> gsi_normal_transforms_;
    std::vector<glm::mat3> gsi_inv_normal_transforms_;
    // GS instance GS index offsets, as well as mesh vertex offsets
    std::vector<int> gsi_gs_index_offsets_;
    // Vertex buffer for regular meshes
    std::vector<Vertex> gsi_vertices_;
    std::vector<int>    gsi_indices_;
    // Index offsets for mesh instances
    std::vector<int> gsi_mesh_index_offsets_;
    // Num indices for mesh instances
    std::vector<int> gsi_mesh_num_indices_;
    // GS instances GS count
    std::vector<int> gsi_gs_counts_;

    std::vector<glm::vec3> gsi_positions_;
    std::vector<glm::vec3> gsi_rotations_;
    std::vector<glm::vec3> gsi_scales_;

    std::vector<bool> gsi_active_;

    std::unique_ptr<DeviceScene> device_scene_;

    // Lights (packed)
    // std::vector<Light> lights_;
    std::vector<LightData> light_data_;
    // Instance transform will be applied to the lights when uploading to device.
    std::vector<int> light_instance_;

    std::vector<SimpleMaterial> gsi_materials_;

    std::filesystem::path environment_map_path_;
};

#endif //INC_3DGS_ADVGI_SCENE_H

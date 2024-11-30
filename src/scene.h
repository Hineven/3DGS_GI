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


class Camera {
public:
    Camera() = default;
    ~Camera() = default;

    CameraDescription PackDescription (int film_width, int film_height) const;

    glm::mat4x4 GetViewMatrix () const;
    glm::mat4x4 GetProjectionMatrix () const;

    glm::vec3 position {0, 0, 0};
    glm::vec3 direction {0, 0, -1};
    glm::vec3 up {0, 1, 0};
    // radians
    float fov_y {1.f};

    // Some windows header macro occupies these identifiers
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

    inline Camera & GetCamera() { return camera_; }

    inline DeviceScene & GetDeviceScene() { return *device_scene_; }

    inline int GetNumGaussians () const { return num_gaussians_; }

    inline int GetNumInstances () const { return num_instances_; }

    ~Scene();

    friend class DeviceScene;
    friend class Renderer;
protected:


    Camera camera_ {};

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
    // GS instance transforms
    std::vector<glm::mat4x3> gsi_transforms_;
    std::vector<glm::mat4x3> gsi_inv_transforms_;
    std::vector<glm::mat3x3> gsi_normal_transforms_;
    // GS instance GS index offsets
    std::vector<int> gsi_gs_index_offsets_;
    // GS instances GS count
    std::vector<int> gsi_gs_counts_;

    std::unique_ptr<DeviceScene> device_scene_;

    std::filesystem::path environment_map_path_;
};

#endif //INC_3DGS_ADVGI_SCENE_H

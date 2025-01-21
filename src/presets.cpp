/*
 * Created: 2025/1/4
 * Author:  hineven
 * See LICENSE for licensing.
 */

#include <glm/matrix.hpp>

#include "app_internal.h"
#include "glm/gtx/matrix_decompose.hpp"

// I know it's dumb. But I have no time to code about reading configuration files.
// I'm gonna coding ugly anyway!

void AppInternal::LoadTeaserScene (std::string env_name, bool load_dragon) {
    auto root_path = GetRootPath();
    if (env_name == "") env_name = "rogland_overcast_4k.exr";
    scene_.LoadEnvironmentMap(root_path + "data/environment_maps/" + env_name);
    // scene_.LoadGaussians(root_path + "data/barn/point_cloud/iteration_50000/point_cloud.ply", true);
    // scene_.LoadGaussians(root_path + "data/hotdog/point_cloud/iteration_50000/point_cloud.ply", true);
    auto inst_family = scene_.LoadGaussians(root_path + "data/family/point_cloud/iteration_50000/point_cloud.ply", true);
    // auto inst_y = scene_.DuplicateInstance(inst_x);
    auto inst_chair = scene_.LoadGaussians(root_path + "data/chair/point_cloud/iteration_50000/point_cloud.ply", true);
    auto inst_w = scene_.LoadGltf(root_path + "data/lighting_room/scene.gltf");
    auto inst_single_truck = scene_.LoadGaussians(root_path + "data/single_truck/point_cloud/iteration_50000/point_cloud.ply", true);
    auto inst_air_baloons = scene_.LoadGaussians(root_path + "data/air_baloons/point_cloud/iteration_50000/point_cloud.ply", true);
    auto inst_jugs = scene_.LoadGaussians(root_path + "data/jugs/point_cloud/iteration_50000/point_cloud.ply", true);
    // auto inst_nerf_chair = scene_.LoadGaussians(root_path + "data/nerf_chair/point_cloud/iteration_40000/point_cloud.ply", true);
    // scene_.LoadGaussians(root_path + "data/single_truck/point_cloud/iteration_50000/point_cloud.ply", true);
    // scene_.LoadGaussians(root_path + "data/caterpillar/point_cloud/iteration_50000/point_cloud.ply", true);
    // scene_.LoadGaussians(root_path + "data/chair/point_cloud/iteration_50000/point_cloud.ply", true);
    // scene_.LoadGaussians(root_path + "data/garden/point_cloud/iteration_7000/point_cloud.ply", true);
    // scene_.LoadGaussians(root_path + "data/counter/point_cloud/iteration_7000/point_cloud.ply", true);

    {
        glm::vec3 position = glm::vec3(-1.786, 0.01, -0.36);
        glm::vec3 rotation = glm::radians(glm::vec3(3, 106, 180));
        glm::vec3 scale = glm::vec3(0.8, 0.8, 0.8);
        scene_.SetInstanceTransform(inst_family, {position, rotation, scale});
    }

    // {
    //     glm::vec3 position = glm::vec3(5, 0, 0);
    //     glm::vec3 rotation = glm::vec3(glm::radians(45.f), glm::radians(30.f), glm::radians(8.f));
    //     glm::vec3 scale = glm::vec3(1.3, 0.6, 0.8);
    //
    //     glm::mat4x3 transform = glm::mat4x3(
    //         glm::translate(glm::mat4(1.f), position)
    //         * glm::toMat4(glm::quat(rotation))
    //         * glm::scale(glm::mat4(1.f), scale)
    //     );
    //     scene_.SetInstanceTransform(inst_y, {position, rotation, scale});
    // }
    {
        glm::vec3 position = glm::vec3(-1.218, -0.962, 0.888);
        glm::vec3 rotation = glm::radians(glm::vec3(-90, 32, 0));
        glm::vec3 scale = glm::vec3(0.5, 0.5, 0.5);
        scene_.SetInstanceTransform(inst_chair, {position, rotation, scale});
    }

    {
        glm::vec3 position = glm::vec3(1.651, -1.49, 1.04);
        glm::vec3 rotation = glm::radians(glm::vec3(-90, -15, 0));
        glm::vec3 scale = glm::vec3(0.6, 0.6, 0.6);
        scene_.SetInstanceTransform(inst_jugs, {position, rotation, scale});
    }

    {
        glm::vec3 position = glm::vec3(1.83, 0.491, -1.261);
        glm::vec3 rotation = glm::radians(glm::vec3(-90, -22, 0));
        glm::vec3 scale = glm::vec3(0.7, 0.7, 0.7);
        scene_.SetInstanceTransform(inst_air_baloons, {position, rotation, scale});
    }

    {
        glm::vec3 position = glm::vec3(0.848, -0.860, -0.5);
        glm::vec3 rotation = glm::radians(glm::vec3(0, -141, 0));
        glm::vec3 scale = glm::vec3(0.7, 0.7, 0.7);
        scene_.SetInstanceTransform(inst_single_truck, {position, rotation, scale});
    }

    if (load_dragon) {
        auto x = scene_.LoadGltf(root_path + "data/chinese_dragon/scene.gltf");
        auto y = x + 1;
        glm::vec3 position = glm::vec3(-0.467, -1.660, 2.384);
        glm::vec3 rotation = glm::radians(glm::vec3(-90, -55, 0));
        glm::vec3 scale = glm::vec3(0.5, 0.5, 0.5);
        scene_.SetInstanceTransform(x, {position, rotation, scale});
        scene_.SetInstanceTransform(y, {position, rotation, scale});
    }

    scene_.SetDirectionalLight(LightData{
        glm::normalize(glm::vec3(-4.7f, 2.5, -1.f)),
        {}, {},
        {7.f, 4.5f, 4.f}
    });

    {
        auto & camera = scene_.GetCamera();
        camera.direction = glm::normalize(glm::vec3{0.37, -0.14, -0.92});
        camera.position  = glm::vec3{-2.38, 0.24, 5.66};
        camera.fov_y = 0.54f;
        // auto right = glm::normalize(glm::cross(camera.direction, abs_up));
        // camera.up = glm::normalize(glm::cross(right, camera.direction));
    }
}

void AppInternal::LoadLightingComparisonScene (std::string model_name, std::string env_name, glm::vec3 extra_rot, glm::vec3 extra_scale) {
    auto root_path = GetRootPath();
    scene_.LoadEnvironmentMap(root_path + "data/environment_maps/" + env_name);
    auto file_path = root_path + "data/" + model_name + "/point_cloud/iteration_50000/point_cloud.ply";
    if(!std::filesystem::exists(file_path)) {
        file_path = root_path + "data/" + model_name + "/point_cloud/iteration_40000/point_cloud.ply";
    }
    auto inst_obj = scene_.LoadGaussians(file_path, true);

    {
        glm::vec3 position = glm::vec3(0);
        glm::vec3 rotation = glm::radians(glm::vec3(-90, 0, 0) + extra_rot);
        glm::vec3 scale = extra_scale;
        scene_.SetInstanceTransform(inst_obj, {position, rotation, scale});
    }

    scene_.SetDirectionalLight(LightData{
        glm::normalize(glm::vec3(0, 1, 0)),
        {}, {},
        {0.f, 0.f, 0.f}
    });

    {
        auto & camera = scene_.GetCamera();
        camera.direction = glm::normalize(glm::vec3{0.6, -0.41, -0.69});
        camera.position  = glm::vec3{-2.46, 1.78, 2.86};
        camera.fov_y = 0.6911112070083618;
        glm::mat4 View = camera.GetViewMatrix();
        for(int i = 0; i < 4; i++)
            for(int j = 0; j < 4; j++)
                std::cout << View[j][i] << ", ";
        // 0.754605, -0, 0.656179, -0.0203416,
        // 0.26847, 0.912471, -0.308741, -0.0807636,
        // -0.598744, 0.409142, 0.688556, -4.17045,
        // 0, 0, 0, 1
    }
}

void AppInternal::LoadMultiModelLightingComparisonScene (std::string env_name) {
    auto root_path = GetRootPath();
    scene_.LoadEnvironmentMap(root_path + "data/environment_maps/" + env_name);

    auto inst_chair = scene_.LoadGaussians(root_path + "data/nerf_chair/point_cloud/iteration_40000/point_cloud.ply", false);
    auto inst_barn = scene_.LoadGaussians(root_path + "data/barn/point_cloud/iteration_50000/point_cloud.ply", false);
    auto inst_ficus = scene_.LoadGaussians(root_path + "data/ficus/point_cloud/iteration_40000/point_cloud.ply", false);
    auto inst_hotdog = scene_.LoadGaussians(root_path + "data/hotdog/point_cloud/iteration_50000/point_cloud.ply", false);

    {
        glm::vec3 position = glm::vec3(-0.413896, -0.059432, -0.089085);
        glm::vec3 rotation = glm::radians(glm::vec3(-90, 0, 0));
        glm::vec3 scale = glm::vec3(1);
        scene_.SetInstanceTransform(inst_chair, {position, rotation, scale});
    }

    {
        glm::vec3 position = glm::vec3(-4, 0, -6);
        glm::vec3 rotation = glm::radians(glm::vec3(0, 0, 0));
        glm::vec3 scale = glm::vec3(4);
        scene_.SetInstanceTransform(inst_barn, {position, rotation, scale});
    }

    {
        // hotdog
        glm::vec3 position {};
        glm::vec3 rotation {};
        glm::vec3 scale {};
        {
            glm::mat4x4 py_matrix = {
                0.0,
                0.245412,
                0.0,
                -0.444334,
                -0.245529,
                0.0,
                0.0,
                -0.258692,
                0.0,
                0.0,
                0.249831,
                -0.982850,
                0.000000,
                0.000000,
                0.000000,
                1.000000
            };
            // row major to column major memory order
            py_matrix = glm::transpose(py_matrix);
            glm::quat orientation;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(py_matrix, scale, orientation, position, skew, perspective);
            rotation = glm::eulerAngles(orientation);
            // We use y-up coordinates, while r3dg use z-up coordinates.
            // r3dg has weird behavior when transforming objects with inconsistent or negative scales...
            // anyway i will just empirically fix it in our program
            rotation.x -= glm::radians(90.f);
            float t = position.z;
            position.z = -position.y;
            position.y = t;
        }
        scene_.SetInstanceTransform(inst_hotdog, {position, rotation, scale});
    }

    {
        // ficus
        glm::vec3 position {};
        glm::vec3 rotation {};
        glm::vec3 scale {};
        {
            glm::mat4x4 py_matrix = {
                0.800000,
                0.000000,
                0.000000,
                0.997946,
                0.000000,
                0.800000,
                0.000000,
                -0.176206,
                0.000000,
                0.000000,
                0.800000,
                -0.163664,
                0.000000,
                0.000000,
                0.000000,
                1.000000
            };
            // row major to column major memory order
            py_matrix = glm::transpose(py_matrix);
            glm::quat orientation;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(py_matrix, scale, orientation, position, skew, perspective);
            rotation = glm::eulerAngles(orientation);
            // We use y-up coordinates, while r3dg use z-up coordinates.
            rotation.x -= glm::radians(90.f);
            float t = position.z;
            position.z = -position.y;
            position.y = t;
        }
        scene_.SetInstanceTransform(inst_ficus, {position, rotation, scale});
    }

    scene_.SetDirectionalLight(LightData{
        glm::normalize(glm::vec3(0, 1, 0)),
        {}, {},
        {0.f, 0.f, 0.f}
    });

    {
        auto & camera = scene_.GetCamera();
        camera.direction = glm::normalize(glm::vec3{0.6, -0.41, -0.69});
        camera.position  = glm::vec3{-2.46, 1.78, 2.86};
        camera.fov_y = 0.6911112070083618;
        glm::mat4 View = camera.GetViewMatrix();
        for(int i = 0; i < 4; i++)
            for(int j = 0; j < 4; j++)
                std::cout << View[j][i] << ", ";
        // 0.754605, -0, 0.656179, -0.0203416,
        // 0.26847, 0.912471, -0.308741, -0.0807636,
        // -0.598744, 0.409142, 0.688556, -4.17045,
        // 0, 0, 0, 1
    }
}

void AppInternal::LoadFaultyArmadilloScene () {
    auto root_path = GetRootPath();
    scene_.LoadEnvironmentMap(root_path + "data/environment_maps/sunset.exr");
    auto file_path = root_path + "data/armadillo/point_cloud/iteration_40000/point_cloud.ply";
    auto inst_obj = scene_.LoadGaussians(file_path, true);

    {
        glm::vec3 position = glm::vec3(0);
        glm::vec3 rotation = glm::radians(glm::vec3(-90, 0, 0));
        glm::vec3 scale = glm::vec3(1);
        scene_.SetInstanceTransform(inst_obj, {position, rotation, scale});
    }

    scene_.SetDirectionalLight(LightData{
        glm::normalize(glm::vec3(0, 1, 0)),
        {}, {},
        {0.f, 0.f, 0.f}
    });

    {
        float f[] = {
            -1.0,-0.0,-0.0,-0.0,
            0.0,0.6790306014854871, 0.7341098831624058, 7.388582933718159e-08,
            0.0, 0.734110002371696,-0.6790306610901322, -4.031129171840728,
            0.0,0.0,0.0,1.0
        };
        glm::mat4 View = {
            f[0], f[1], f[2], f[3],
            f[4], f[5], f[6], f[7],
            f[8], f[9], f[10], f[11],
            f[12], f[13], f[14], f[15]
        };
        View = glm::transpose(View);
        for(int i = 0; i < 4; i++)
            for(int j = 0; j < 4; j++)
                std::cout << View[j][i] << ", ";
        glm::mat4 InvView = glm::inverse(View);

        glm::vec3 direction = glm::mat3(InvView) * glm::vec3(0, 0, -1);
        glm::vec3 position = glm::vec3(InvView[3]);


        auto & camera = scene_.GetCamera();
        camera.direction = direction;
        camera.position  = position;
        camera.fov_y = 0.6911112070083618;
    }
}

void AppInternal::LoadCornellBoxScene (std::string model_name, glm::vec3 extra_rot, glm::vec3 extra_scale) {
    auto root_path = GetRootPath();
    scene_.LoadEnvironmentMap("");
    auto file_path = root_path + "data/" + model_name + "/point_cloud/iteration_50000/point_cloud.ply";
    if(!std::filesystem::exists(file_path)) {
        file_path = root_path + "data/" + model_name + "/point_cloud/iteration_40000/point_cloud.ply";
    }
    auto inst_obj = scene_.LoadGaussians(file_path, true);

    {
        glm::vec3 position = glm::vec3(0);
        glm::vec3 rotation = glm::radians(glm::vec3(-90, 0, 0) + extra_rot);
        glm::vec3 scale = extra_scale;
        scene_.SetInstanceTransform(inst_obj, {position, rotation, scale});
    }

    auto inst_box = scene_.LoadGltf(root_path + "data/cornell_box/scene.gltf");

    scene_.SetDirectionalLight(LightData{
        glm::normalize(glm::vec3(0, 1, 0)),
        {}, {},
        {0.f, 0.f, 0.f}
    });

    {
        auto & camera = scene_.GetCamera();
        camera.direction = glm::normalize(glm::vec3{0, 0, -1});
        camera.position  = glm::vec3{0, 0, 7.8};
        camera.fov_y = 0.6911112070083618;
    }
    // resolution: 1088x1088
}

void AppInternal::LoadAllLightsScene(bool single_light) {
    auto root_path = GetRootPath();
    scene_.LoadEnvironmentMap(root_path + "data/environment_maps/rogland_overcast_4k.exr");
    if (single_light) scene_.LoadGltf(root_path + "data/all_lights_single/scene.gltf");
    else scene_.LoadGltf(root_path + "data/all_lights/scene.gltf");
    auto inst_r = scene_.LoadGaussians(root_path + "data/armadillo/point_cloud/iteration_40000/point_cloud.ply", false);
    auto inst_g = scene_.LoadGaussians(root_path + "data/armadillo/point_cloud/iteration_40000/point_cloud.ply", false);
    auto inst_b = scene_.LoadGaussians(root_path + "data/armadillo/point_cloud/iteration_40000/point_cloud.ply", false);

    scene_.SetDirectionalLight(LightData{
        glm::normalize(glm::vec3(0, 1, -0.3)),
        {}, {},
        {7.f, 5.5f, 5.f}
    });

    {
        glm::vec3 position = glm::vec3(0, -0.6, -0.4);
        glm::vec3 rotation = glm::radians(glm::vec3(-90, 180, 0));
        glm::vec3 scale = glm::vec3(1, 1, 1);
        scene_.SetInstanceTransform(inst_r, {position, rotation, scale});
    }

    {
        glm::vec3 position = glm::vec3(1.7, -0.6, 0);
        glm::vec3 rotation = glm::radians(glm::vec3(-90, 160, 0));
        glm::vec3 scale = glm::vec3(1, 1, 1);
        scene_.SetInstanceTransform(inst_g, {position, rotation, scale});
    }
    scene_.OverwriteGaussianAlbedo(inst_g, glm::vec3(0.4, 1, 0.4));
    scene_.OverwriteGaussianRoughness(inst_g, 0.99);

    {
        glm::vec3 position = glm::vec3(-1.7, -0.6, 0);
        glm::vec3 rotation = glm::radians(glm::vec3(-90, -160, 0));
        glm::vec3 scale = glm::vec3(1, 1, 1);
        scene_.SetInstanceTransform(inst_b, {position, rotation, scale});
    }
    scene_.OverwriteGaussianAlbedo(inst_b, glm::vec3(0.4, 0.4, 1));
    scene_.OverwriteGaussianRoughness(inst_b, 0.01);

    {
        auto & camera = scene_.GetCamera();
        camera.direction = glm::normalize(glm::vec3{0, 0, -1});
        camera.position  = glm::vec3{0, 0, 7.8};
        camera.fov_y = 0.6911112070083618 * (560.f / 1088);
        // resolution: 1088 x 560
    }
}

void AppInternal::LoadLightRoomScene(std::string model_name, std::string env_name, glm::vec3 extra_pos, glm::vec3 extra_rot, glm::vec3 extra_scale) {
    auto root_path = GetRootPath();
    if (!env_name.empty())
        scene_.LoadEnvironmentMap(root_path + "data/environment_maps/" + env_name + ".exr");
    else scene_.LoadEnvironmentMap("");
    scene_.LoadGltf(root_path + "data/lightroom/scene.gltf");
    auto path = root_path + "data/" + model_name + "/point_cloud/iteration_50000/point_cloud.ply";
    if (!std::filesystem::exists(path))
        path = root_path + "data/" + model_name + "/point_cloud/iteration_40000/point_cloud.ply";
    auto inst_mt = scene_.LoadGaussians(path, false);

    scene_.SetDirectionalLight(LightData{
        glm::normalize(glm::vec3(0, 1, 0)),
        {}, {},
        {0.f, 0.f, 0.f}
    });

    {
        glm::vec3 position = extra_pos;
        glm::vec3 rotation = glm::radians(glm::vec3(-90, 0, 0) + extra_rot);
        glm::vec3 scale = extra_scale;
        scene_.SetInstanceTransform(inst_mt, {position, rotation, scale});
    }

    // {
    //     auto & camera = scene_.GetCamera();
    //     camera.direction = glm::normalize(glm::vec3{-0.69, -0.25, 0.69});
    //     camera.position  = glm::vec3{6, 3.6, -6};
    //     camera.fov_y = 0.6911112070083618;
    //     // resolution: 800 x 800
    // }

    {
        auto & camera = scene_.GetCamera();
        camera.direction = glm::normalize(glm::vec3{-0.22, -0.20, 0.95});
        camera.position  = glm::vec3{1.02, 2.86, -5.62};
        camera.fov_y = 0.6911112070083618;
        // resolution: 800 x 800
    }
}

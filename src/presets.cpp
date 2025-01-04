/*
 * Created: 2025/1/4
 * Author:  hineven
 * See LICENSE for licensing.
 */

#include "app_internal.h"

// I know it's dumb. Anyway I have no time to code about reading configuration files.

void AppInternal::LoadTeaserScene () {
    auto root_path = GetRootPath();
    scene_.LoadEnvironmentMap(root_path + "data/environment_maps/rogland_overcast_4k.exr");
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
        glm::vec3 position = glm::vec3(-1.786, 0.01, -0.092);
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
        glm::vec3 position = glm::vec3(-0.938, -1.3, 1.969);
        glm::vec3 rotation = glm::radians(glm::vec3(-90, 32, 0));
        glm::vec3 scale = glm::vec3(0.5, 0.5, 0.5);
        scene_.SetInstanceTransform(inst_chair, {position, rotation, scale});
    }

    {
        glm::vec3 position = glm::vec3(1.651, -1.49, 1.372);
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
        glm::vec3 position = glm::vec3(0.848, -0.860, -0.796);
        glm::vec3 rotation = glm::radians(glm::vec3(0, -141, 0));
        glm::vec3 scale = glm::vec3(0.7, 0.7, 0.7);
        scene_.SetInstanceTransform(inst_single_truck, {position, rotation, scale});
    }

    scene_.SetDirectionalLight(LightData{
        glm::normalize(glm::vec3(-4.7f, 2.5, -1.f)),
        {}, {},
        {7.f, 4.5f, 4.f}
    });

    {
        auto & camera = scene_.GetCamera();
        camera.direction = glm::normalize(glm::vec3{0, 0, -1.f});
        camera.position  = glm::vec3{0, -0.40, 7.8};
        // auto right = glm::normalize(glm::cross(camera.direction, abs_up));
        // camera.up = glm::normalize(glm::cross(right, camera.direction));
    }
}

void AppInternal::LoadLightingComparisonScene (std::string model_name, std::string env_name) {
    auto root_path = GetRootPath();
    scene_.LoadEnvironmentMap(root_path + "data/environment_maps/" + env_name);
    auto inst_obj = scene_.LoadGaussians(root_path + "data/" + model_name + "/point_cloud/iteration_50000/point_cloud.ply", true);

    {
        glm::vec3 position = glm::vec3(0);
        glm::vec3 rotation = glm::radians(glm::vec3(-90, 0, 0));
        glm::vec3 scale = glm::vec3(1, 1, 1);
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
//        camera.direction = glm::normalize(glm::vec3{0, 0, -1.f});
//        camera.position  = glm::vec3{0, 0, 3};
        glm::mat4 View = camera.GetViewMatrix();
        for(int i = 0; i < 4; i++)
          for(int j = 0; j < 4; j++)
            std::cout << View[j][i] << ", ";
    }

//    float camera_transform_mat [] = {
//        0.97814757,
//        -0.20791169,
//        0,
//        -0.18319818,

//        -0.041450925,
//        -0.19501127,
//        -0.97992474,
//        -0.27414584,

//        0.20373780,
//        0.95851099e,
//        -0.19936794,
//        5.8470416,

//        0.0000000,
//        0.0000000,
//        0.0000000,
//        -1.0000000
//    };
//    glm::mat4x4 camera_viewproj = glm::make_mat4(camera_transform_mat);
    // Z axis inverted for my coordinate system (OpenGL lhs vs D3D rhs)
}
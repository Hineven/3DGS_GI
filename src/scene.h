/*
 * Created: 2024/11/7
 * Author:  hineven
 * See LICENSE for licensing.
 */

#ifndef INC_3DGS_ADVGI_SCENE_H
#define INC_3DGS_ADVGI_SCENE_H

#include <vector>
#include <glm/glm.hpp>

class Scene {
public:
    Scene();
    ~Scene();
protected:

    std::vector<glm::vec3> gs_positions_;
    std::vector<glm::vec3> gs_colors_;
    std::vector<float> gs_alphas_;
    std::vector<glm::vec3> gs_rotations_;
    std::vector<glm::vec3> gs_scales_;


};

#endif //INC_3DGS_ADVGI_SCENE_H

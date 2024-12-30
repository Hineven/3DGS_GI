/*
 * Created: 2024/11/9
 * Author:  hineven
 * See LICENSE for licensing.
 */

#ifndef INC_3DGS_ADVGI_DEVICE_SCENE_H
#define INC_3DGS_ADVGI_DEVICE_SCENE_H

#include <gfx.h>
#include <gfx_scene.h>
#include "scene.h"
class DeviceScene {
public:

    DeviceScene ();

    ~DeviceScene () ;

    void Upload (const Scene & scene) ;
    void Destroy ();

    void Bind  (const GfxProgram & program) ;


    // TODO compressed format

    GfxBuffer gaussian_position;
    GfxBuffer gaussian_alpha;
    GfxBuffer gaussian_rotation;
    GfxBuffer gaussian_scale;
    GfxBuffer gaussian_color;
    GfxBuffer gaussian_sh1;
    GfxBuffer gaussian_sh2;
    GfxBuffer gaussian_sh3;

    GfxBuffer gaussian_normal;
    GfxBuffer gaussian_albedo;
    GfxBuffer gaussian_roughness;

    GfxAccelerationStructure acceleration_structure_;
    // Each primitive is a GS instance (group)
    std::vector<GfxRaytracingPrimitive> rt_primitives_;

    // Offset of the gaussian index inside a gs instance / group
    GfxBuffer gsi_instance_base_;
    // Number of gaussians in a gs instance / group
    GfxBuffer gsi_instance_count_;
    // Transforms packed in mat4x3, local to world
    GfxBuffer gsi_transform_;
    // World to local
    GfxBuffer gsi_inv_transform_;
    // To world normal transform, mat3x3 packed
    GfxBuffer gsi_normal_transform_;
    GfxBuffer gsi_inv_normal_transform_;

    GfxBuffer gsi_vertices_;
    GfxBuffer gsi_indices_;
    GfxBuffer gsi_mesh_index_offsets_;
    GfxBuffer gsi_mesh_num_indices_;

    GfxBuffer gsi_materials_;

    GfxScene gfx_scene_;

    GfxTexture environment_map_;

    GfxBuffer light_count_;
    GfxBuffer light_;
    GfxBuffer light_data_;

    GfxBuffer light_data_staging_;
    GfxBuffer transform_data_staging_;

    void UpdateLights (const Scene & scene);

    void UpdateTransforms (const Scene & scene);

protected:

    GfxProgram ibl_program_;
    GfxKernel draw_sky_kernel_;
    GfxKernel blur_sky_kernel_;

    void UpdateGfxScene (const Scene & scene);

};

#endif //INC_3DGS_ADVGI_DEVICE_SCENE_H

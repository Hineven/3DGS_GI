/*
 * Created: 2024/11/9
 * Author:  hineven
 * See LICENSE for licensing.
 */

#ifndef INC_3DGS_ADVGI_DEVICE_SCENE_H
#define INC_3DGS_ADVGI_DEVICE_SCENE_H

#include "gfx.h"
#include "scene.h"
class DeviceScene {
public:

    ~DeviceScene () ;

    void Upload (const Scene & scene) ;
    void Destroy ();

    void Bind  (const GfxProgram & program) ;

    GfxBuffer gaussian_position;
    GfxBuffer gaussian_alpha;
    GfxBuffer gaussian_rotation;
    GfxBuffer gaussian_scale;
    GfxBuffer gaussian_color;
    GfxBuffer gaussian_sh1;
    GfxBuffer gaussian_sh2;
    GfxBuffer gaussian_sh3;

    GfxAccelerationStructure acceleration_structure_;
    // All gaussians in the scene
    // TODO split primitives ?
    GfxRaytracingPrimitive rt_primitive_;

};

#endif //INC_3DGS_ADVGI_DEVICE_SCENE_H

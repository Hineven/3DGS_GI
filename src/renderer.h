/*
 * Created: 2024/11/9
 * Author:  hineven
 * See LICENSE for licensing.
 */

#ifndef INC_3DGS_ADVGI_RENDERER_H
#define INC_3DGS_ADVGI_RENDERER_H
#include <gfx.h>
#include "common.h"
#include "timed.h"
#include "app_internal.h"
class Renderer : public Timed {
public:
    Renderer();
    ~Renderer();

    bool Initialize ();
    void Destroy ();

    void Render();

    void RenderUI();

protected:
    bool CreateResources ();
    bool CreateKernels ();
    void DestroyResources ();
void DestroyKernels ();

    void GenerateDispatchIndirect (const GfxBuffer & thread_count_buffer);
    void GenerateDispatchRaysIndirect (const GfxBuffer & thread_count_buffer);


    struct {
        GfxBuffer dispatch_indirect_command;
        GfxBuffer dispatch_rays_indirect_command;

        GfxBuffer gaussian_active_count;
        GfxBuffer active_gaussian_list;
        GfxBuffer active_gaussian_depth;
        GfxBuffer active_gaussian_screen_position;
        GfxBuffer active_gaussian_screen_radius;
        GfxBuffer active_gaussian_conic_w;
        GfxBuffer active_gaussian_tile_count;
        GfxBuffer active_gaussian_instance_base;
        GfxBuffer active_gaussian_instance_count;
        GfxBuffer active_gaussian_color;

        GfxBuffer active_gaussian_instance_key;
        GfxBuffer active_gaussian_instance_key_sorted;
        GfxBuffer active_gaussian_instance_gaussian_index;
        GfxBuffer active_gaussian_instance_gaussian_index_sorted;

        GfxBuffer tile_gaussian_instance_start;
        GfxBuffer tile_gaussian_instance_end;

        GfxBuffer ray_to_trace_count;
        GfxBuffer ray_to_trace_direction;
        GfxBuffer ray_to_trace_origin;
        GfxBuffer ray_to_trace_t_max;
        GfxBuffer ray_to_trace_flags;

        GfxBuffer ray_to_trace_result;

        GfxBuffer UB;
    } buf_ {};

    struct {
        GfxTexture G_color;
//        GfxTexture output;
    } tex_ {};

    struct {
        // Used to write the vertex/index buffer for ray traced 3dgs proxy meshes.
        GfxKernel GenerateRTMesh;

        GfxKernel GenerateDispatchIndirect;

        GfxKernel ClearCounters;
        GfxKernel TransformAndSplatGaussians;
        GfxKernel ShadeActiveGaussians;
        GfxKernel SetActiveGaussianInstanceCount;
        GfxKernel AssignGaussianInstanceKeys;
        GfxKernel FindTileGaussianInstanceStarts;
        GfxKernel RasterizeActiveGaussians;

        GfxKernel Trace3DGSRays;
        GfxKernel SpawnCameraRays;
        GfxKernel DisplayCameraRays;

        GfxKernel TonemapAndDraw;
    } kernel_ {};

    struct {
        // Maximum number of rays to trace in 1 dispatch.
        int max_num_rays {1920 * 1080};

        // Use HWRT to render the 3dgs scene.
        bool show_HWRT_color {false};

        // The scaling of the proxy geometry in 3DGS ray tracing.
        // The original paper says 0.3 is good.
        float gaussian_RT_proxy_geometry_sigma {0.3f};

        // Minimum opacity at the ray-gayssian intersection for 3DGS to be evaluated in ray tracing.
        // Otherwise, they are ignored.
        float min_alpha_for_gaussian_evaluation {0.01f};
    } options_;

    struct {
        int wave_lane_count {};
    } cfg_;

    GfxProgram program_ {};

    GfxSbt sbt_ {};

    int frame_index_ {};

    bool should_build_acceleration_structure_ {true};
};

#endif //INC_3DGS_ADVGI_RENDERER_H

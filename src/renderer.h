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
#include "bluenoise.h"

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
    void GenerateDrawIndirect (const GfxBuffer & vertex_count_buffer);

    BlueNoiseSampler blue_noise_sampler_;

    struct {
        GfxBuffer dispatch_indirect_command;
        GfxBuffer dispatch_rays_indirect_command;
        GfxBuffer draw_indirect_command;

        GfxBuffer active_gaussian_count;
        GfxBuffer active_gaussian_list_src;
        GfxBuffer active_gaussian_list;
        GfxBuffer active_gaussian_linear_depth_src;
        GfxBuffer active_gaussian_linear_depth;

        GfxBuffer active_gaussian_NDC_position;
        GfxBuffer active_gaussian_quad_NDC_vector0;
        GfxBuffer active_gaussian_quad_NDC_vector1;

        GfxBuffer active_gaussian_color;

        GfxBuffer ray_count;
        GfxBuffer ray_to_trace_count[2];
        GfxBuffer ray_to_trace_list[2];
        GfxBuffer ray_to_trace_direction;
        GfxBuffer ray_to_trace_origin;
        GfxBuffer ray_to_trace_seed;
        GfxBuffer ray_to_trace_flags;

        GfxBuffer ray_to_trace_result;

        GfxBuffer UB;
    } buf_ {};

    struct {

        // fpXx2 (LinearDepth, depth normalization weight)
        // note: depth normalization weight is different from opacity.
        // opacity is stored in w component of G_albedo_alpha
        GfxTexture G_depth;
        // unorm8x4
        GfxTexture G_albedo_alpha;
        // unorm8, roughness
        GfxTexture G_material;
        // unorm8x4
        GfxTexture G_normal;
        // Full precision R32 depth (filtered from R16 linear depth)
        // 0 for infinitely far
        GfxTexture G_filtered_depth;
        // Z depth ([0, 1] range, -1 for infinitely far)
        GfxTexture G_zdepth[2];
        // Min hi-z buffer
        GfxTexture near_HZB;

        // fp16x4
        GfxTexture radiance[2];
//        GfxTexture output;
    } tex_ {};

    struct {
        // Used to write the vertex/index buffer for ray traced 3dgs proxy meshes.
        GfxKernel GenerateRTMesh;

        GfxKernel GenerateDispatchRaysIndirect;
        GfxKernel GenerateDispatchIndirect;
        GfxKernel GenerateDrawIndirect;

        GfxKernel ClearCounters;
        GfxKernel FilterActiveGaussians;
        GfxKernel ProjectActiveGaussians;
        GfxKernel ResolveGBuffers;
        GfxKernel FilterDepth;
        GfxKernel GenerateNearHZB;
        GfxKernel ReconstructNormals;
        GfxKernel SampleLightRays;
        GfxKernel TraceRaysInScreenSpace;
        GfxKernel CompactRayTraces;
        GfxKernel ResolveDirectLighting;
        GfxKernel FinalComposition;

        // Trace shading rays
        GfxKernel Trace3DGSRays;
        // Trace shadow rays
        GfxKernel Trace3DGSShadowRays;
        GfxKernel SpawnCameraRays;
        GfxKernel DisplayCameraRays;

        GfxKernel DrawActiveGaussians;
        GfxKernel TonemapAndDraw;
    } kernel_ {};

    struct {
        // Maximum number of rays to trace in 1 dispatch.
        int max_num_rays {4 * 1024 * 1024};

        // Visualize HWRT results (dispatch a bunch of camera rays and visualize)
        bool visualize_HWRT {false};
        // Visualize HWRT shading ray results. Otherwise, visualize shadow ray depth results.
        bool visualize_HWRT_shading_rays {false};

        // The scaling of the proxy geometry in 3DGS ray tracing.
        // The original paper says 0.3 is good.
        float gaussian_RT_proxy_geometry_sigma {0.3f};

        // Minimum opacity at the ray-gayssian intersection for 3DGS to be evaluated in ray tracing.
        // Otherwise, they are ignored.
        float HWRT_min_alpha_for_gaussian_evaluation {0.01f};

        // Pixels with alpha values higher than this threshold are considered opaque.
        float opaque_threshold {0.05f};

        // The clip value for depth rasterization. Making it smoother.
        float depth_alpha_clip_value {0.08f};

        // The quality of stochastic ray tracing [0, 1].
        // Lower values bring more biased but faster results.
        float stochastic_ray_tracing_quality {0.25f};

        // Render color only when doing rasterization
        bool no_G_buffers {false};

        // Normals are reconstructed from the depth buffer rather than rasterized from gaussians.
        bool reconstruct_normals {false};

        DXGI_FORMAT depth_format {DXGI_FORMAT_R16G16_FLOAT};

        float3 debug_light_position {0, 0, 0};

        uint debug_mode {0};
    } options_;

    struct {
        int wave_lane_count {};
    } cfg_;

    GfxProgram program_ {};

    GfxSbt sbt_ {};

    int frame_index_ {};

    bool should_build_acceleration_structure_ {true};

    bool need_reload_shaders_ {false};

    UniformBlock previous_UB_;
};

#endif //INC_3DGS_ADVGI_RENDERER_H

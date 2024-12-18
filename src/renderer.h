/*
 * Created: 2024/11/9
 * Author:  hineven
 * See LICENSE for licensing.
 */

#ifndef INC_3DGS_ADVGI_RENDERER_H
#define INC_3DGS_ADVGI_RENDERER_H
#include <map>
#include <gfx.h>
#include <random>

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

        // Light grid
        GfxBuffer LightGrid_grid_light_list_allocator;
        GfxBuffer LightGrid_grid_light_count;
        GfxBuffer LightGrid_grid_light_list_offset;
        GfxBuffer LightGrid_grid_light_list;

        // Rasterization
        GfxBuffer active_gaussian_count;
        GfxBuffer active_gaussian_list_src;
        GfxBuffer active_gaussian_list;
        GfxBuffer active_gaussian_linear_depth_src;
        GfxBuffer active_gaussian_linear_depth;

        GfxBuffer active_gaussian_NDC_position;
        GfxBuffer active_gaussian_quad_NDC_vector0;
        GfxBuffer active_gaussian_quad_NDC_vector1;

        GfxBuffer active_gaussian_color;

        // Raytracing
        GfxBuffer ray_count;
        GfxBuffer ray_to_trace_count[2];
        GfxBuffer ray_to_trace_list[2];
        GfxBuffer ray_to_trace_direction;
        GfxBuffer ray_to_trace_origin;
        GfxBuffer ray_to_trace_UV_position;
        GfxBuffer ray_to_trace_seed;
        GfxBuffer ray_to_trace_flags;

        GfxBuffer ray_to_trace_result;

        // Direct illumintion
        GfxBuffer direct_illumination_ray_occlusion_threshold;
        GfxBuffer direct_illumination_ray_contribution;

        // Uniform block
        GfxBuffer UB;

#ifndef NDEBUG
        GfxBuffer Debug_direct_illumination_pixel_ray_index;
        GfxBuffer Debug_visualize_ray_count;
        GfxBuffer Debug_visualize_ray_vertex;
        GfxBuffer Debug_visualize_ray_color;
        GfxBuffer Debug_visualize_ray_ray_index;
#endif
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
        // fp16x4
        GfxTexture G_emission_alpha;
        // Full precision R32 depth (filtered from R16 linear depth)
        // 0 for infinitely far
        GfxTexture G_filtered_depth;
        // Z depth ([0, 1] range, -1 for infinitely far)
        GfxTexture G_zdepth[2];
        // Min hi-z buffer
        GfxTexture near_HZB;

        // fp16x4
        GfxTexture debug;

        // fp16x4
        GfxTexture direct_illumination[2];
        GfxTexture filtered_direct_illumination;
        // fp16x4
        GfxTexture radiance[2];

        // The depth buffer used for rasterization
        // Cull gaussian fragments falling behind regular geometries.
        GfxTexture rasterization_depth;
    } tex_ {};

    struct {
        // Used to write the vertex/index buffer for ray traced 3dgs proxy meshes.
        GfxKernel GenerateRTMesh;

        GfxKernel GenerateDispatchRaysIndirect;
        GfxKernel GenerateDispatchIndirect;
        GfxKernel GenerateDrawIndirect;

        GfxKernel ClearCounters;
        GfxKernel DrawAreaLights;
        GfxKernel FilterActiveGaussians;
        GfxKernel ProjectActiveGaussians;
        GfxKernel DrawActiveGaussians;
        GfxKernel ResolveGBuffers;
        GfxKernel FilterDepth;
        GfxKernel CombineGBuffers;
        GfxKernel GenerateNearHZB;
        GfxKernel ReconstructNormals;
        GfxKernel InitializeCounters;
        GfxKernel UpdateLightHeaders;
        GfxKernel InjectLights;
        GfxKernel SampleLightRays;
        GfxKernel TraceRaysInScreenSpace;
        GfxKernel CompactRayTraces;
        GfxKernel ResolveDirectLighting;
        GfxKernel SpatialFilterDirectIllumination[2];
        GfxKernel TemporalFilterDirectIllumination;
        GfxKernel FinalComposition;

        // Trace shading rays
        GfxKernel Trace3DGSRays;
        // Trace shadow rays
        GfxKernel Trace3DGSShadowRays;
        GfxKernel DirectIlluminationTrace3DGSShadowRays;
        GfxKernel SpawnCameraRays;
        GfxKernel DisplayCameraRays;

        GfxKernel TonemapAndDraw;
    } kernel_ {};

    struct {
        // Maximum number of rays to trace in 1 dispatch.
        int max_num_rays {4 * 1024 * 1024};

        // Visualize HWRT results (dispatch a bunch of camera rays and visualize)
        bool visualize_HWRT {false};

        // The width/height/depth of the light grid. should not exceed LIGHT_GRID_MAX_GRID_SIZE
        int light_grid_size {16};

        int light_grid_max_num_entries {256 * 1024};

        // Number of cascades of the light grid, should not exceed LIGHT_GRID_MAX_NUM_CASCADES
        int light_grid_num_cascades {3};

        // Spatial radius for all denoising. Defined as a shader macro for loop unrolling.
        int filter_radius {2};

        // Render color only when doing rasterization
        bool no_G_buffers {false};

        bool SSRT_enable {true};

        bool HWRT_enable {true};

        // Normals are reconstructed from the depth buffer rather than rasterized from gaussians.
        bool reconstruct_normals {true};

        DXGI_FORMAT depth_format {DXGI_FORMAT_R16G16_FLOAT};

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

    struct {
        glm::vec3 directional_light_dir;
        glm::vec3 directional_light_color;
        int area_light_count {0};
        float area_light_sizes [10];
        glm::vec3 area_lights_facing [10];
        glm::vec3 area_light_positions[10];
        glm::vec3 area_light_local_vertices[30];
        glm::vec3 area_light_colors [10];
    } CB {};

    UniformBlock UB {};
    UniformBlock history_UB_ {};

    struct CVar {
        std::string name;
        std::string desc;
        enum type {
            BOOL,
            FLOAT,
            INT,
            VEC2,
            VEC3
        } type {};
        union {
            glm::vec3 f3;
            glm::vec2 f2;
            float f;
            int i;
        } v {};
        double mn;
        double mx;
    };
    std::map<void*, CVar> cvar_;
    bool auto_switch_debug_ = false;

    std::mt19937 rng_;

    inline float nextFloat () {
        return std::uniform_real_distribution<float>(0, 1)(rng_);
    }
};

#endif //INC_3DGS_ADVGI_RENDERER_H

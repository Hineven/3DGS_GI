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

    void ComputeShadingLUT ();

    void GenerateDispatchIndirect (const GfxBuffer & thread_count_buffer);
    void GenerateDispatchRaysIndirect (const GfxBuffer & thread_count_buffer);
    void GenerateDrawIndirect (const GfxBuffer & vertex_count_buffer);

    GfxBuffer staging_buffer_[kGfxConstant_BackBufferCount];
    int staging_buffer_frame_index_[kGfxConstant_BackBufferCount];
    int staging_buffer_frame_offset_[kGfxConstant_BackBufferCount];
    void UploadBufferStaged (GfxBuffer buf, const void * data, size_t size);
    void ResetStagingBuffers ();

    BlueNoiseSampler blue_noise_sampler_;


    // -----------------------------
    // Device resource handles
    // -----------------------------
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
        GfxBuffer active_gaussian_list;
        GfxBuffer active_gaussian_indirect;
        GfxBuffer active_gaussian_indirect_src;
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

        // Mesh cards
        GfxBuffer card_sets;
        GfxBuffer cards;

        // Uniform block
        GfxBuffer UB_pool;

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
        GfxTexture indirect_illumination[2];
        GfxTexture filtered_indirect_illumination;
        // fp16x4
        GfxTexture radiance[2];

        // RGBA8 (max card res)
        GfxTexture card_workspace_color_alpha;
        // RGBA8 (max card res)
        GfxTexture card_workspace_normal;
        // RG32F (max card res)
        GfxTexture card_workspace_linear_depth;


        GfxTexture card_atlas_color;
        GfxTexture card_atlas_alpha;
        GfxTexture card_atlas_normal;
        GfxTexture card_atlas_linear_depth;
        // RGBA16F
        GfxTexture card_atlas_direct_illumination;
        // RGBA16F
        GfxTexture card_atlas_indirect_illumination;
        GfxTexture card_atlas_lighting;
        // (max atlas page res)
        // Used for rendering / updating / filtering canvas...etc.
        // I don't want to code more... so the update is only limited to 1 page of the
        // atlas at a time. (anyway 32x32 large tiles in 1 page should be enough for small scenes)
        GfxTexture card_workspace_direct_illumination[2];
        GfxTexture card_workspace_indirect_illumination[2];
        GfxTexture card_workspace_lighting[2];


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
        GfxKernel SSRC_ReInsertHashGridTiles;
        GfxKernel SSRC_AllocateUniformProbes;
        GfxKernel SSRC_AllocateAdaptiveProbes;
        GfxKernel SSRC_PrepareProbeProcessing;
        GfxKernel SSRC_ResetProbeTexels;
        GfxKernel SSRC_ReprojectProbeHistory;
        GfxKernel SSRC_AllocateProbeUpdateRays;
        GfxKernel SSRC_SetRayCounts;
        GfxKernel SSRC_SampleProbeUpdateRay;
        GfxKernel TraceRaysInScreenSpaceForSSRC;
        // Trace3DGSProbeUpdateRays
        GfxKernel SSRC_ResolveRayDepths;
        GfxKernel SSRC_ResolveHitLightingFromScreenHistory;
        GfxKernel SSRC_SampleLightRays;
        GfxKernel SSRC_PrepareClearNewHashGridTileCells;
        GfxKernel SSRC_ClearNewHashGridTileCells;
        // Trace3DGSShadowRaysWithoutIndirectionList
        GfxKernel SSRC_ResolveHitDirectLightingFromTraceResult;
        GfxKernel SSRC_FilterHashGrids;
        GfxKernel SSRC_ResolveProbeUpdateRayRadianceFromCells;
        GfxKernel SSRC_UpdateProbes;
        GfxKernel SSRC_FilterProbes;
        GfxKernel SSRC_PadProbeTextureEdges;
        GfxKernel SSRC_IntegrateASG;
        GfxKernel TemporalDenoiseLighting;
        GfxKernel FinalComposition;

        GfxKernel ClearCard;
        GfxKernel FilterActiveGaussiansForCard;
        GfxKernel ProjectActiveGaussiansForCard;
        GfxKernel DrawActiveGaussiansForCard;
        GfxKernel ResolveGBuffersForCard;
        GfxKernel CopyCardToAtlas;

        // Trace shading rays
        GfxKernel Trace3DGSRays;
        // Trace shadow rays
        GfxKernel Trace3DGSShadowRays;
        GfxKernel DirectIlluminationTrace3DGSShadowRays;
        GfxKernel Trace3DGSProbeUpdateRays;
        GfxKernel SpawnCameraRays;
        GfxKernel DisplayCameraRays;
        GfxKernel VisualizeMeshCardScene;
        GfxKernel VisualizeMeshCardAtlas;

        GfxKernel TonemapAndDraw;
    } kernel_ {};

    GfxProgram program_ {};
    GfxSbt sbt_ {};

    // ----------------------------
    // Options (may change per frame)
    // ----------------------------
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

    // ----------------------------
    // Uniform buffer allocator
    // ----------------------------
    GfxBuffer AllocateUBForCurrentFrame (size_t size) ;
    template<typename T>
    inline GfxBuffer AllocateUBForCurrentFrame (int count = 1) {
        // Align to 256 bytes for uniform buffers.
        auto stride = roundUp((uint32_t)sizeof(T), 256u);
        GfxBuffer buf = AllocateUBForCurrentFrame(stride * count);
        buf.setStride(stride);
        return buf;
    }
    // Allocation sizes for each frame.
    int UB_pool_allocation_sizes_ [kGfxConstant_BackBufferCount] {};
    int UB_pool_allocation_frames_ [kGfxConstant_BackBufferCount] {};
    int UB_pool_allocation_offset_ {};

    void ResetUniformBufferPool ();


    // ----------------------------
    // Configuration (consistent across frames)
    // ----------------------------
    struct {
        int wave_lane_count {};
        int uniform_buffer_size {4 * 1024 * 1024};
        int max_num_instances {256};
    } cfg_;

    // Frame flags and states
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

    // ----------------------------
    // Meshcards related host data
    // ----------------------------
    struct {
        std::vector<int> card_set_remove_requests {}; // instance id
        std::vector<int> card_set_add_requests {}; // instance id
        std::vector<int> card_set_redraw_requests {}; // instance id
        // Ugly, Just brute-force it!
        int atlas_occupancy [NUM_CARD_ATLAS][CARD_ATLAS_RESOLUTION / MIN_CARD_RESOLUTION][CARD_ATLAS_RESOLUTION / MIN_CARD_RESOLUTION];
        std::vector<CardSet> card_sets {};
        std::vector<Card> cards {};
    } MC {}; // Meshcards related host data

    void RequestRedrawAllMeshCardsForAllInstances ();

    // ----------------------------
    // micro CVar system
    // ----------------------------
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

    // ----------------------------
    // Misc
    // ----------------------------
    // host random number generator
    std::mt19937 rng_;
    inline float nextFloat () {
        return std::uniform_real_distribution<float>(0, 1)(rng_);
    }

    // UI related persistent flags
    bool auto_switch_debug_ = false;
};

#endif //INC_3DGS_ADVGI_RENDERER_H

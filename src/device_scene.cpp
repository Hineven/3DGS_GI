/*
 * Created: 2024/11/9
 * Author:  hineven
 * See LICENSE for licensing.
 */
#include <bit>
#include "device_scene.h"
#include "app_internal.h"
#include "glm/gtc/packing.inl"

DeviceScene::DeviceScene() {
    auto gfx = AppInternal::GetInstance().GetGfx();
    auto root_path = AppInternal::GetInstance().GetRootPath();
    ibl_program_ = gfxCreateProgram(gfx, "src/shaders/ibl", root_path.c_str());
    app_assert(ibl_program_);
    GfxDrawState draw_sky_state = {};
    gfxDrawStateSetColorTarget(draw_sky_state, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
    draw_sky_kernel_ =
        gfxCreateGraphicsKernel(gfx, ibl_program_, draw_sky_state, "DrawSky");
    blur_sky_kernel_ = gfxCreateComputeKernel(gfx, ibl_program_, "BlurSky");
}

DeviceScene::~DeviceScene () {
    Destroy();
    auto gfx = AppInternal::GetInstance().GetGfx();
    gfxDestroyKernel(gfx, draw_sky_kernel_);
    gfxDestroyKernel(gfx, blur_sky_kernel_);
    gfxDestroyProgram(gfx, ibl_program_);
}

void DeviceScene::Upload (const Scene & scene) {
    auto &gfx = AppInternal::GetInstance().GetGfx();
    Destroy();

    std::cout << "Uploading scene to device" << std::endl;
    gaussian_position = gfxCreateBuffer(
        gfx, scene.num_gaussians_ * sizeof(glm::vec3), scene.gs_positions_.data()
    );
    gaussian_position.setName("GaussianPositionBuffer");
    gaussian_alpha = gfxCreateBuffer(
        gfx, scene.num_gaussians_ * sizeof(float), scene.gs_alphas_.data()
    );
    gaussian_alpha.setName("GaussianAlphaBuffer");
    gaussian_rotation = gfxCreateBuffer(
        gfx, scene.num_gaussians_ * sizeof(glm::vec4), scene.gs_rotations_.data()
    );
    gaussian_rotation.setName("GaussianRotationBuffer");
    gaussian_scale = gfxCreateBuffer(
        gfx, scene.num_gaussians_ * sizeof(glm::vec3), scene.gs_scales_.data()
    );
    gaussian_scale.setName("GaussianScaleBuffer");
    if (scene.gs_colors_.size()) {
        gaussian_color = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * sizeof(glm::vec3), scene.gs_colors_.data()
        );
        gaussian_color.setName("GaussianColorBuffer");
        gaussian_sh1 = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * 3 * sizeof(glm::vec3), scene.gs_sh1_.data()
        );
        gaussian_sh1.setName("GaussianSH1Buffer");
        gaussian_sh2 = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * 5 * sizeof(glm::vec3), scene.gs_sh2_.data()
        );
        gaussian_sh2.setName("GaussianSH2Buffer");
        gaussian_sh3 = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * 7 * sizeof(glm::vec3), scene.gs_sh3_.data()
        );
        gaussian_sh3.setName("GaussianSH3Buffer");
    }
    if (scene.gs_albedos_.size()) {
        gaussian_albedo = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * sizeof(glm::vec3), scene.gs_albedos_.data()
        );
        gaussian_albedo.setName("GaussianAlbedoBuffer");
        gaussian_roughness = gfxCreateBuffer(
            gfx, scene.num_gaussians_ * sizeof(float), scene.gs_roughnesses_.data()
        );
        gaussian_roughness.setName("GaussianRoughnessBuffer");
    }
    gaussian_normal = gfxCreateBuffer(
        gfx, scene.num_gaussians_ * sizeof(glm::vec3), scene.gs_normals_.data()
    );
    gaussian_normal.setName("GaussianNormalBuffer");

    gsi_instance_base_ = gfxCreateBuffer(gfx, scene.num_instances_ * sizeof(uint32_t), scene.gsi_gs_index_offsets_.data());
    gsi_instance_base_.setName("GSInstanceBaseBuffer");
    gsi_instance_count_ = gfxCreateBuffer(gfx, scene.num_instances_ * sizeof(uint32_t), scene.gsi_gs_counts_.data());
    gsi_instance_count_.setName("GSInstanceCountBuffer");

    // Regulare meshes
    gsi_mesh_num_indices_ = gfxCreateBuffer(gfx, scene.num_instances_ * sizeof(uint32_t), scene.gsi_mesh_num_indices_.data());
    gsi_mesh_num_indices_.setName("MeshNumIndicesBuffer");
    gsi_mesh_index_offsets_ = gfxCreateBuffer(gfx, scene.num_instances_ * sizeof(uint32_t), scene.gsi_mesh_index_offsets_.data());
    gsi_mesh_index_offsets_.setName("MeshIndexOffsetsBuffer");

    gsi_vertices_ = gfxCreateBuffer(gfx, scene.gsi_vertices_.size() * sizeof(Vertex), scene.gsi_vertices_.data());
    gsi_vertices_.setName("VerticesBuffer");
    gsi_vertices_.setStride(sizeof(Vertex));
    gsi_indices_ = gfxCreateBuffer(gfx, scene.gsi_indices_.size() * sizeof(uint32_t), scene.gsi_indices_.data());
    gsi_indices_.setName("IndicesBuffer");

    // Simple materials
    gsi_materials_ = gfxCreateBuffer(gfx, scene.num_instances_ * sizeof(SimpleMaterial), scene.gsi_materials_.data());
    gsi_materials_.setName("MaterialsBuffer");

    UpdateTransforms(scene);

    UpdateLights(scene);

    UpdateGfxScene(scene);

}

void DeviceScene::UpdateLights(const Scene &scene) {
    auto gfx = AppInternal::GetInstance().GetGfx();
    int num_lights = scene.GetNumLights();
    if (!light_count_) {
        light_count_ = gfxCreateBuffer<uint>(gfx, 1, &num_lights);
        light_count_.setName("LightCountBuffer");
    }
    if (light_.getSize() != num_lights * sizeof(uint2)) {
        light_ = gfxCreateBuffer<uint2>(gfx, num_lights);
        light_.setName("LightBuffer");
    }
    if (light_data_.getSize() != num_lights * 4 * sizeof(float3)) {
        light_data_ = gfxCreateBuffer<float3>(gfx, num_lights * 4, scene.light_data_.data());
        light_data_.setName("LightDataBuffer");
    }
    if (light_data_staging_.getSize() != num_lights * 4 * sizeof(float3)) {
        light_data_staging_ = gfxCreateBuffer<float3>(gfx, num_lights * 4, nullptr, kGfxCpuAccess_Write);
        light_data_staging_.setName("LightDataStagingBuffer");
    }
    gfxCommandClearBuffer(gfx, light_count_, num_lights);
    std::vector<LightData> transformed_light_data(num_lights);
    // Apply instance transforms to the light if they are attached to an instance
    for (int i = 0; i < num_lights; i++) {
        if (scene.light_instance_[i] != -1) {
            glm::mat4x3 transform = scene.GetInstanceTransformMatrix(scene.light_instance_[i]);
            transformed_light_data[i] = scene.light_data_[i];
            transformed_light_data[i].V1 = transform * glm::vec4(transformed_light_data[i].V1, 1.f);
            transformed_light_data[i].V2 = transform * glm::vec4(transformed_light_data[i].V2, 1.f);
            transformed_light_data[i].V3 = transform * glm::vec4(transformed_light_data[i].V3, 1.f);
        } else {
            transformed_light_data[i] = scene.light_data_[i];
        }
    }
    memcpy(gfxBufferGetData(gfx, light_data_staging_), transformed_light_data.data(), num_lights * 4 * sizeof(float3));
    gfxCommandCopyBuffer(gfx, light_data_, light_data_staging_);
}

void DeviceScene::UpdateTransforms(const Scene &scene) {
    auto gfx = AppInternal::GetInstance().GetGfx();
    auto total_size = uint32_t(2 * scene.num_instances_ * (sizeof(glm::mat4x3) + sizeof(glm::mat3x3)));
    if (transform_data_staging_.getSize() < total_size) {
        transform_data_staging_ = gfxCreateBuffer(gfx, total_size, nullptr, kGfxCpuAccess_Write);
        transform_data_staging_.setName("TransformDataStagingBuffer");
    }
    uint32_t offset = 0;
    if (!gsi_transform_ || gsi_transform_.getSize() < scene.num_instances_ * sizeof(glm::mat4x3)) {
        if (gsi_transform_) gfxDestroyBuffer(gfx, gsi_transform_);
        gsi_transform_ = gfxCreateBuffer(gfx, scene.num_instances_ * sizeof(glm::mat4x3), scene.gsi_transforms_.data());
        gsi_transform_.setName("GSITransformBuffer");
    } else {
        memcpy((char*)gfxBufferGetData(gfx, transform_data_staging_) + offset, scene.gsi_transforms_.data(), scene.num_instances_ * sizeof(glm::mat4x3));
        gfxCommandCopyBuffer(gfx, gsi_transform_, 0, transform_data_staging_, offset, scene.num_instances_ * sizeof(glm::mat4x3));
    }
    offset += scene.num_instances_ * sizeof(glm::mat4x3);
    if (!gsi_inv_transform_ || gsi_inv_transform_.getSize() < scene.num_instances_ * sizeof(glm::mat4x3)) {
        if (gsi_inv_transform_) gfxDestroyBuffer(gfx, gsi_inv_transform_);
        gsi_inv_transform_ = gfxCreateBuffer(gfx, scene.num_instances_ * sizeof(glm::mat4x3), scene.gsi_inv_transforms_.data());
        gsi_inv_transform_.setName("GSIInvTransformBuffer");
    } else {
        memcpy((char*)gfxBufferGetData(gfx, transform_data_staging_) + offset, scene.gsi_inv_transforms_.data(), scene.num_instances_ * sizeof(glm::mat4x3));
        gfxCommandCopyBuffer(gfx, gsi_inv_transform_, 0, transform_data_staging_, offset, scene.num_instances_ * sizeof(glm::mat4x3));
    }
    offset += scene.num_instances_ * sizeof(glm::mat4x3);
    if (!gsi_normal_transform_ || gsi_normal_transform_.getSize() < scene.num_instances_ * sizeof(glm::mat3x3)) {
        if (gsi_normal_transform_) gfxDestroyBuffer(gfx, gsi_normal_transform_);
        gsi_normal_transform_ = gfxCreateBuffer(gfx, scene.num_instances_ * sizeof(glm::mat3x3), scene.gsi_normal_transforms_.data());
        gsi_normal_transform_.setName("GSINormalTransformBuffer");
    } else {
        memcpy((char*)gfxBufferGetData(gfx, transform_data_staging_) + offset, scene.gsi_normal_transforms_.data(), scene.num_instances_ * sizeof(glm::mat3x3));
        gfxCommandCopyBuffer(gfx, gsi_normal_transform_, 0,  transform_data_staging_, offset, scene.num_instances_ * sizeof(glm::mat3x3));
    }
    offset += scene.num_instances_ * sizeof(glm::mat3x3);
    if (!gsi_inv_normal_transform_ || gsi_inv_normal_transform_.getSize() < scene.num_instances_ * sizeof(glm::mat3x3)) {
        if (gsi_inv_normal_transform_) gfxDestroyBuffer(gfx, gsi_inv_normal_transform_);
        gsi_inv_normal_transform_ = gfxCreateBuffer(gfx, scene.num_instances_ * sizeof(glm::mat3x3), scene.gsi_inv_normal_transforms_.data());
        gsi_inv_normal_transform_.setName("GSIInvNormalTransformBuffer");
    } else {
        memcpy((char*)gfxBufferGetData(gfx, transform_data_staging_) + offset, scene.gsi_inv_normal_transforms_.data(), scene.num_instances_ * sizeof(glm::mat3x3));
        gfxCommandCopyBuffer(gfx, gsi_inv_normal_transform_, 0, transform_data_staging_, offset, scene.num_instances_ * sizeof(glm::mat3x3));
    }
}

void DeviceScene::UpdateGfxScene(const Scene & scene) {
    auto gfx = AppInternal::GetInstance().GetGfx();
    if (!gfx_scene_) gfx_scene_ = gfxCreateScene();
    auto filename = scene.environment_map_path_.string();
    if (kGfxResult_NoError != gfxSceneImport(gfx_scene_, filename.c_str())) {
        app_warning("Failed to import environment map: " << scene.environment_map_path_.string().c_str());
        return ;
    }
    auto resource = gfxSceneFindObjectByAssetFile<GfxImage>(gfx_scene_, filename.c_str());
    if (!resource) {
        app_warning("Failed to load environment map: " << scene.environment_map_path_.string().c_str());
        return ;
    }
    uint width = 1024;
    uint num_mips = gfxCalculateMipCount(width);

    environment_map_ = gfxCreateTextureCube(
        gfx, width, DXGI_FORMAT_R16G16B16A16_FLOAT, num_mips
    );
    environment_map_.setName("EnvironmentMap");

    uint in_width = resource->width;
    uint in_height = resource->height;
    uint in_num_mips = gfxCalculateMipCount(in_width, in_height);
    uint in_num_channels = resource->channel_count;
    uint in_channel_bytes = resource->bytes_per_channel;

    GfxTexture in_environment_texture = gfxCreateTexture2D(
        gfx, in_width, in_height, resource->format, in_num_mips
    );
    {
        GfxBuffer upload_buffer = gfxCreateBuffer(gfx,
        (size_t)in_width * in_height * in_num_channels * in_channel_bytes,
            resource->data.data(), kGfxCpuAccess_Write);
        gfxCommandCopyBufferToTexture(gfx, in_environment_texture, upload_buffer);
        gfxCommandGenerateMips(gfx, in_environment_texture);
        gfxDestroyBuffer(gfx, upload_buffer);
    }

    glm::dvec3 const forward_vectors[] = {glm::dvec3(-1.0, 0.0, 0.0), glm::dvec3(1.0, 0.0, 0.0),
    glm::dvec3(0.0, 1.0, 0.0), glm::dvec3(0.0, -1.0, 0.0), glm::dvec3(0.0, 0.0, -1.0),
    glm::dvec3(0.0, 0.0, 1.0)};

    glm::dvec3 const up_vectors[] = {glm::dvec3(0.0, -1.0, 0.0), glm::dvec3(0.0, -1.0, 0.0),
        glm::dvec3(0.0, 0.0, -1.0), glm::dvec3(0.0, 0.0, 1.0), glm::dvec3(0.0, -1.0, 0.0),
        glm::dvec3(0.0, -1.0, 0.0)};

    uint32_t const buffer_dimensions[] = {
        environment_map_.getWidth(), environment_map_.getHeight()};
    gfxProgramSetParameter(gfx, ibl_program_, "g_BufferDimensions", buffer_dimensions);
    gfxProgramSetParameter(gfx, ibl_program_, "g_EnvironmentMap", in_environment_texture);
    gfxProgramSetParameter(gfx, ibl_program_, "g_LinearSampler", AppInternal::GetInstance().GetSamplers().linear_wrap);

    for (uint32_t cubemap_face = 0; cubemap_face < 6; ++cubemap_face)
    {

        gfxCommandBindColorTarget(gfx, 0, environment_map_, 0, cubemap_face);

        glm::dmat4 const view =
            glm::lookAt(glm::dvec3(0.0), forward_vectors[cubemap_face], up_vectors[cubemap_face]);
        glm::dmat4 const proj          = glm::perspective(M_PI / 2.0, 1.0, 0.1, 1e4);
        glm::mat4 const  view_proj_inv = glm::mat4(glm::inverse(proj * view));

        gfxProgramSetParameter(gfx, ibl_program_, "g_ViewProjectionInverse", view_proj_inv);

        gfxCommandBindKernel(gfx, draw_sky_kernel_);
        gfxCommandDraw(gfx, 3);
    }

    for (uint32_t mip_level = 1; mip_level < num_mips; ++mip_level)
    {
        gfxProgramSetParameter(
            gfx, ibl_program_, "g_InEnvironmentBuffer", environment_map_, mip_level - 1);
        gfxProgramSetParameter(
            gfx, ibl_program_, "g_OutEnvironmentBuffer", environment_map_, mip_level);

        uint32_t const *num_threads = gfxKernelGetNumThreads(gfx, blur_sky_kernel_);
        uint32_t const  num_groups_x =
            (GFX_MAX(width >> mip_level, 1u) + num_threads[0] - 1) / num_threads[0];
        uint32_t const num_groups_y =
            (GFX_MAX(width >> mip_level, 1u) + num_threads[1] - 1) / num_threads[1];
        uint32_t const num_groups_z = 6; // blur all faces

        gfxCommandBindKernel(gfx, blur_sky_kernel_);
        gfxCommandDispatch(gfx, num_groups_x, num_groups_y, num_groups_z);
    }

    auto handle = gfxSceneGetImageHandle(gfx_scene_, resource.getIndex());
    gfxSceneDestroyImage(gfx_scene_, handle);
    gfxDestroyTexture(gfx, in_environment_texture);
}



void DeviceScene::Destroy () {
    auto & gfx = AppInternal::GetInstance().GetGfx();
    gfxDestroyBuffer(gfx, gaussian_position);
    gfxDestroyBuffer(gfx, gaussian_alpha);
    gfxDestroyBuffer(gfx, gaussian_rotation);
    gfxDestroyBuffer(gfx, gaussian_scale);
    gfxDestroyBuffer(gfx, gaussian_color);
    gfxDestroyBuffer(gfx, gaussian_sh1);
    gfxDestroyBuffer(gfx, gaussian_sh2);
    gfxDestroyBuffer(gfx, gaussian_sh3);
    gfxDestroyBuffer(gfx, gaussian_albedo);
    gfxDestroyBuffer(gfx, gaussian_roughness);
    gfxDestroyBuffer(gfx, gaussian_normal);
    gfxDestroyBuffer(gfx, gsi_instance_base_);
    gfxDestroyBuffer(gfx, gsi_instance_count_);
    gfxDestroyBuffer(gfx, gsi_transform_);
    gfxDestroyBuffer(gfx, gsi_inv_transform_);
    gfxDestroyBuffer(gfx, gsi_normal_transform_);
    gfxDestroyBuffer(gfx, gsi_inv_normal_transform_);

    gfxDestroyBuffer(gfx, gsi_vertices_);
    gfxDestroyBuffer(gfx, gsi_indices_);
    gfxDestroyBuffer(gfx, gsi_mesh_index_offsets_);
    gfxDestroyBuffer(gfx, gsi_mesh_num_indices_);
    gfxDestroyBuffer(gfx, gsi_materials_);

    gfxDestroyTexture(gfx, environment_map_);

    gfxDestroyBuffer(gfx, light_count_);
    gfxDestroyBuffer(gfx, light_);
    gfxDestroyBuffer(gfx, light_data_);

    gfxDestroyBuffer(gfx, light_data_staging_);
    gfxDestroyBuffer(gfx, transform_data_staging_);

    gfxDestroyScene(gfx_scene_);
}

void DeviceScene::Bind(const GfxProgram &program) {
    auto & gfx = AppInternal::GetInstance().GetGfx();

    gfxProgramSetParameter(gfx, program, "g_EnvironmentMap", environment_map_);

    gfxProgramSetParameter(gfx, program, "g_GaussianPositionBuffer", gaussian_position);
    gfxProgramSetParameter(gfx, program, "g_GaussianAlphaBuffer", gaussian_alpha);
    gfxProgramSetParameter(gfx, program, "g_GaussianRotationBuffer", gaussian_rotation);
    gfxProgramSetParameter(gfx, program, "g_GaussianScaleBuffer", gaussian_scale);
    if (gaussian_color) {
        gfxProgramSetParameter(gfx, program, "g_GaussianColorBuffer", gaussian_color);
        gfxProgramSetParameter(gfx, program, "g_GaussianSH1Buffer", gaussian_sh1);
        gfxProgramSetParameter(gfx, program, "g_GaussianSH2Buffer", gaussian_sh2);
        gfxProgramSetParameter(gfx, program, "g_GaussianSH3Buffer", gaussian_sh3);
    }
    if (gaussian_albedo) {
        gfxProgramSetParameter(gfx, program, "g_GaussianAlbedoBuffer", gaussian_albedo);
        gfxProgramSetParameter(gfx, program, "g_GaussianRoughnessBuffer", gaussian_roughness);
    }
    gfxProgramSetParameter(gfx, program, "g_GaussianNormalBuffer", gaussian_normal);
    gfxProgramSetParameter(gfx, program, "g_InstanceGaussianIndexOffsetBuffer", gsi_instance_base_);
    gfxProgramSetParameter(gfx, program, "g_InstanceGaussianCountBuffer", gsi_instance_count_);
    gfxProgramSetParameter(gfx, program, "g_InstanceTransformBuffer", gsi_transform_);
    gfxProgramSetParameter(gfx, program, "g_InstanceInvTransformBuffer", gsi_inv_transform_);
    gfxProgramSetParameter(gfx, program, "g_InstanceNormalTransformBuffer", gsi_normal_transform_);
    gfxProgramSetParameter(gfx, program, "g_InstanceInvNormalTransformBuffer", gsi_inv_normal_transform_);

    // Meshes
    gfxProgramSetParameter(gfx, program, "g_VertexBuffer", gsi_vertices_);
    gfxProgramSetParameter(gfx, program, "g_IndexBuffer", gsi_indices_);
    gfxProgramSetParameter(gfx, program, "g_MeshIndexOffsetsBuffer", gsi_mesh_index_offsets_);
    gfxProgramSetParameter(gfx, program, "g_MeshIndexCountsBuffer", gsi_mesh_num_indices_);
    gfxProgramSetParameter(gfx, program, "g_MaterialBuffer", gsi_materials_);

    // Lights
    gfxProgramSetParameter(gfx, program, "g_LightCountBuffer", light_count_);
    gfxProgramSetParameter(gfx, program, "g_LightBuffer", light_);
    gfxProgramSetParameter(gfx, program, "g_LightDataBuffer", light_data_);
}

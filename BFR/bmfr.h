#pragma once


#include <vector>
#include <vulkan/vulkan.h>
#include "vk_utils.h"

#define BENCHMARK
#ifdef BENCHMARK
#include <chrono>
#endif

class Denoiser {
public:
    Denoiser(VkUtil::Vk_Context& context, uint32_t image_width, uint32_t image_height, uint32_t kernel_x = 16, uint32_t kernel_y = 16);

    ~Denoiser();

    void set_noisy_image(VkImage image, VkImageView image_view);
    void set_depth_image(VkImage image, VkImageView image_view);
    void set_normals_image(VkImage image, VkImageView image_view);
    void set_albedo_image(VkImage image, VkImageView image_view);
    void set_previous_depth_image(VkImage image, VkImageView image_view);
    void set_previous_normals_image(VkImage image, VkImageView image_view);

    void set_camera_matrix(float matrix[16]);
    void set_prev_camera_matrix(float matrix[16]);
    void set_frame_number(uint32_t number);

    /* uses a one time command buffer as for every denoise call it is expected that new images have to be bound*/
    void denoise();
    void get_noise_acc(void* data, uint32_t byte_size);
    void get_data_acc(void* data, uint32_t byte_size);
    void get_denoised(void* data, uint32_t byte_size);

private:
    struct ShaderData {
        float prev_cam_matrix[16];
        float cur_cam_matrix[16];
        int pixel_offset[4];
        uint32_t frame_number;
        uint32_t padding[3];
    };
    // external vulkan resources (have to be deleted externally)
    VkUtil::Vk_Context vk_context;
    VkImage image_cur_albedo, image_cur_normal, image_cur_depth, image_cur_noisy, image_prev_normal, image_prev_depth;
    VkImageView imageView_cur_albedo, imageView_cur_normal, imageView_cur_depth, imageView_cur_noisy, imageView_prev_normal, imageView_prev_depth;
    // internal vulkan resources (have to be deleted)
    VkPipeline pipeline_bmfr, pipeline_taa;
    VkPipelineLayout pipeline_layou_bmfr, pipeline_layout_taa;
    VkDescriptorSetLayout descriptor_set_layout_bmfr, descriptor_set_layout_taa;
    VkDescriptorSet descriptor_set_bmfr, descriptor_set_taa;
    VkCommandBuffer denoise_commands;
    uint32_t offset_images[5];
    VkImage image_spp[2], image_acc[2], image_prevUV;
    VkImageView imageView_spp[2], imageView_acc[2], imageView_prevUV;
    VkSampler sampler_image;
    VkDeviceMemory memory_images;
    VkBuffer buffer_descriptor;
    VkDeviceMemory memory_buffer;
    VkQueryPool query_pool;

    VkBuffer debug_buffer;
    VkDeviceMemory debug_memory;

    uint32_t image_width;
    uint32_t image_height;
    uint32_t kernel_x;
    uint32_t kernel_y;

    uint32_t frame_number;
    float cam_matrix[16];
    float prev_cam_matrix[16];
    std::vector<std::pair<int,int>> pixel_offsets;

    static char string_bmfr[];
    static char string_taa[];

    void create_pipelines();
    void create_images();
    void create_buffers();
    void create_sampler();
    void create_descriptor_set();
};
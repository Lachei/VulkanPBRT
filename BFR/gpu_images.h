#pragma once
#include "vk_utils.h"

class GpuDoubleImage {
public:
    GpuDoubleImage(VkUtil::Vk_Context& vk_context, int width, int height, VkFormat image_format, VkImageUsageFlags image_usage, VkMemoryPropertyFlags memory_properties);
    ~GpuDoubleImage();

    // swaps current and previous image
    void swap_images();
    void get_cur_image(VkImage& image, VkImageView& imageView);
    void get_prev_image(VkImage& image, VkImageView& imageView);

    void set_cur_image(void* data, uint32_t byte_size);
    void get_cur_image(void* data, uint32_t byte_size);
private:
    VkUtil::Vk_Context vk_context;
    VkFormat format_image;
    uint32_t offset_images[2];
    VkImage images[2];
    VkImageView imageViews[2];
    VkDeviceMemory memory_images;

    uint32_t current_image_index;
    uint32_t width, height;
};
#include "gpu_images.h"

GpuDoubleImage::GpuDoubleImage(VkUtil::Vk_Context& vk_context, int width, int height, VkFormat image_format, VkImageUsageFlags image_usage, VkMemoryPropertyFlags memory_properties):
    vk_context(vk_context), format_image(image_format), width(width), height(height), current_image_index(0)
{
    /* creating the images*/
    VkUtil::create_image(vk_context, width, height, image_format, image_usage, images);
    VkUtil::create_image(vk_context, width, height, image_format, image_usage, images + 1);

    VkMemoryAllocateInfo memAlloc{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    VkMemoryRequirements memReq;
    offset_images[0] = 0;
    vkGetImageMemoryRequirements(vk_context.device, images[0], &memReq);
    memAlloc.allocationSize = memReq.size;
    offset_images[1] = memAlloc.allocationSize;
    vkGetImageMemoryRequirements(vk_context.device, images[1], &memReq);
    memAlloc.allocationSize += memReq.size;
    memAlloc.memoryTypeIndex = VkUtil::find_memory_type(vk_context, memReq.memoryTypeBits, memory_properties);
    VkResult err = vkAllocateMemory(vk_context.device, &memAlloc, vk_context.allocator, &memory_images);    VkUtil::check_vk_result(err);

    err = vkBindImageMemory(vk_context.device, images[0], memory_images, offset_images[0]);                 VkUtil::check_vk_result(err);
    err = vkBindImageMemory(vk_context.device, images[1], memory_images, offset_images[1]);                 VkUtil::check_vk_result(err);

    VkUtil::create_image_view(vk_context, images[0], image_format, 1, VK_IMAGE_ASPECT_COLOR_BIT, imageViews);
    VkUtil::create_image_view(vk_context, images[1], image_format, 1, VK_IMAGE_ASPECT_COLOR_BIT, imageViews + 1);
    
    /* transform image layout to general layout*/
    VkCommandBuffer commands;
    VkUtil::create_command_buffer(vk_context, &commands);
    VkUtil::transition_image_layout(commands, images[0], image_format, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkUtil::transition_image_layout(commands, images[1], image_format, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkUtil::commit_command_buffer(vk_context, commands);
}

GpuDoubleImage::~GpuDoubleImage()
{
    if (images[0]) vkDestroyImage(vk_context.device, images[0], vk_context.allocator);
    if (images[1]) vkDestroyImage(vk_context.device, images[1], vk_context.allocator);
    if (imageViews[0]) vkDestroyImageView(vk_context.device, imageViews[0], vk_context.allocator);
    if (imageViews[1]) vkDestroyImageView(vk_context.device, imageViews[1], vk_context.allocator);
    if (memory_images) vkFreeMemory(vk_context.device, memory_images, vk_context.allocator);
}

void GpuDoubleImage::swap_images()
{
    current_image_index ^= 1;
}

void GpuDoubleImage::get_cur_image(VkImage& image, VkImageView& imageView)
{
    image = images[current_image_index];
    imageView = imageViews[current_image_index];
}

void GpuDoubleImage::get_prev_image(VkImage& image, VkImageView& imageView)
{
    image = images[current_image_index ^ 1];
    imageView = imageViews[current_image_index ^ 1];
}

void GpuDoubleImage::set_cur_image(void* data, uint32_t byte_size)
{
    VkUtil::upload_image_data(vk_context, images[current_image_index], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, format_image, width, height, 1, 0, data, byte_size);
}

void GpuDoubleImage::get_cur_image(void* data, uint32_t byte_size)
{
    VkUtil::download_image_data(vk_context, images[current_image_index], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, format_image, width, height, 1, 0, data, byte_size);
}

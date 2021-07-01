#include "bmfr.h"
#include <thread>

char Denoiser::string_bmfr[] = "shaders/bmfr.comp.spv";
char Denoiser::string_taa[] = "shaders/taa.comp.spv";

Denoiser::Denoiser(VkUtil::Vk_Context& context, uint32_t image_width, uint32_t image_height, uint32_t kernel_x, uint32_t kernel_y):
    vk_context(context), image_width(image_width), image_height(image_height), kernel_x(kernel_x), kernel_y(kernel_y)
{
    pixel_offsets = { {-7, -11}, {-14, -8}, {-5, -12}, {-15, -1}, {-5, -9}, {-1, -4}, {-14, -7}, {0, -13}, {-5, -1}, {-1, 0}, {-15, -2}, {-14, -10}, {-1, -1}, {-6, -3}, {0, -8}, {-10, -4} };

    VkQueryPoolCreateInfo query_pool_info;
    query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    query_pool_info.pNext = nullptr;
    query_pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    query_pool_info.queryCount = 2;
    query_pool_info.pipelineStatistics = 0;
    query_pool_info.flags = 0;

    VkResult res = vkCreateQueryPool(context.device, &query_pool_info, nullptr, &query_pool);

    create_pipelines();
    create_images();
    create_buffers();
    create_sampler();
    create_descriptor_set();
}

Denoiser::~Denoiser()
{
    vkQueueWaitIdle(vk_context.queue);      //this is added to wait for all jobs pending in the queue to be finished
    if (pipeline_bmfr) vkDestroyPipeline(vk_context.device, pipeline_bmfr, vk_context.allocator);
    if (pipeline_taa) vkDestroyPipeline(vk_context.device, pipeline_taa, vk_context.allocator);

    if (pipeline_layou_bmfr) vkDestroyPipelineLayout(vk_context.device, pipeline_layou_bmfr, vk_context.allocator);
    if (pipeline_layout_taa) vkDestroyPipelineLayout(vk_context.device, pipeline_layout_taa, vk_context.allocator);

    if (descriptor_set_layout_bmfr) vkDestroyDescriptorSetLayout(vk_context.device, descriptor_set_layout_bmfr, vk_context.allocator);
    if (descriptor_set_layout_taa) vkDestroyDescriptorSetLayout(vk_context.device, descriptor_set_layout_taa, vk_context.allocator);

    if (image_acc[0]) vkDestroyImage(vk_context.device, image_acc[0], vk_context.allocator);
    if (image_acc[1]) vkDestroyImage(vk_context.device, image_acc[1], vk_context.allocator);
    if (image_spp[0]) vkDestroyImage(vk_context.device, image_spp[0], vk_context.allocator);
    if (image_spp[1]) vkDestroyImage(vk_context.device, image_spp[1], vk_context.allocator);
    if (image_prevUV) vkDestroyImage(vk_context.device, image_prevUV, vk_context.allocator);
    if (imageView_acc[0]) vkDestroyImageView(vk_context.device, imageView_acc[0], vk_context.allocator);
    if (imageView_acc[1]) vkDestroyImageView(vk_context.device, imageView_acc[1], vk_context.allocator);
    if (imageView_spp[0]) vkDestroyImageView(vk_context.device, imageView_spp[0], vk_context.allocator);
    if (imageView_spp[1]) vkDestroyImageView(vk_context.device, imageView_spp[1], vk_context.allocator);
    if (imageView_prevUV) vkDestroyImageView(vk_context.device, imageView_prevUV, vk_context.allocator);
    if (memory_images) vkFreeMemory(vk_context.device, memory_images, vk_context.allocator);
    if (sampler_image) vkDestroySampler(vk_context.device, sampler_image, vk_context.allocator);

    if (buffer_descriptor) vkDestroyBuffer(vk_context.device, buffer_descriptor, vk_context.allocator);
    if (memory_buffer) vkFreeMemory(vk_context.device, memory_buffer, vk_context.allocator);

    if (query_pool) vkDestroyQueryPool(vk_context.device, query_pool, vk_context.allocator);
}

void Denoiser::set_noisy_image(VkImage image, VkImageView image_view)
{
    image_cur_noisy = image;
    imageView_cur_noisy = image_view;
}

void Denoiser::set_depth_image(VkImage image, VkImageView image_view)
{
    image_cur_depth = image;
    imageView_cur_depth = image_view;
}

void Denoiser::set_normals_image(VkImage image, VkImageView image_view)
{
    image_cur_normal = image;
    imageView_cur_normal = image_view;
}

void Denoiser::set_albedo_image(VkImage image, VkImageView image_view)
{
    image_cur_albedo = image;
    imageView_cur_albedo = image_view;
}

void Denoiser::set_previous_depth_image(VkImage image, VkImageView image_view)
{
    image_prev_depth = image;
    imageView_prev_depth = image_view;
}

void Denoiser::set_previous_normals_image(VkImage image, VkImageView image_view)
{
    image_prev_normal = image;
    imageView_prev_normal = image_view;
}

void Denoiser::set_camera_matrix(float matrix[16])
{
    std::copy_n(matrix, 16, cam_matrix);
}

void Denoiser::set_prev_camera_matrix(float matrix[16])
{
    std::copy_n(matrix, 16, prev_cam_matrix);
}

void Denoiser::set_frame_number(uint32_t number)
{
    frame_number = number;
}

void Denoiser::denoise()
{
    uint32_t frame_index = frame_number & 1;

    ShaderData shaderData;
    std::copy_n(prev_cam_matrix, 16, shaderData.prev_cam_matrix);
    std::copy_n(cam_matrix, 16, shaderData.cur_cam_matrix);
    std::copy_n(&pixel_offsets[frame_number%pixel_offsets.size()].first, 2, &shaderData.pixel_offset[0]);
    shaderData.frame_number = frame_number;

    VkUtil::upload_data(vk_context, memory_buffer, 0, sizeof(ShaderData), &shaderData);

    VkCommandBuffer commands;
    VkUtil::create_command_buffer(vk_context, &commands);

    /* filling the bmfr descriptor set*/
    uint32_t binding = 0;
    VkUtil::update_image_descriptor_set(vk_context, NULL, imageView_spp[frame_index], VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, binding++, descriptor_set_bmfr);
    VkUtil::update_image_descriptor_set(vk_context, NULL, imageView_acc[frame_index], VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, binding++, descriptor_set_bmfr);
    VkUtil::update_image_descriptor_set(vk_context, NULL, imageView_prevUV, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, binding++, descriptor_set_bmfr);
    VkUtil::update_image_descriptor_set(vk_context, sampler_image, imageView_cur_albedo, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding++, descriptor_set_bmfr);
    VkUtil::update_image_descriptor_set(vk_context, sampler_image, imageView_cur_normal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding++, descriptor_set_bmfr);
    VkUtil::update_image_descriptor_set(vk_context, sampler_image, imageView_cur_depth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding++, descriptor_set_bmfr);
    VkUtil::update_image_descriptor_set(vk_context, sampler_image, imageView_cur_noisy, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding++, descriptor_set_bmfr);
    VkUtil::update_image_descriptor_set(vk_context, sampler_image, imageView_prev_normal, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding++, descriptor_set_bmfr);
    VkUtil::update_image_descriptor_set(vk_context, sampler_image, imageView_prev_depth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding++, descriptor_set_bmfr);
    VkUtil::update_image_descriptor_set(vk_context, sampler_image, imageView_spp[frame_index ^ 1], VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding++, descriptor_set_bmfr);
    VkUtil::update_image_descriptor_set(vk_context, sampler_image, imageView_acc[frame_index ^ 1], VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding++, descriptor_set_bmfr);
    VkUtil::update_descriptor_set(vk_context, buffer_descriptor, sizeof(ShaderData), binding++, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptor_set_bmfr);
    //VkUtil::update_descriptor_set(vk_context, debug_buffer, 256 * 13 * sizeof(float), binding++, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptor_set_bmfr);

    /* filling the taa descriptor set*/
    binding = 0;
    VkUtil::update_image_descriptor_set(vk_context, sampler_image, imageView_prevUV, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding++, descriptor_set_taa);
    VkUtil::update_image_descriptor_set(vk_context, sampler_image, imageView_acc[frame_index^1], VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding++, descriptor_set_taa);
    VkUtil::update_image_descriptor_set(vk_context, NULL, imageView_acc[frame_index], VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, binding++, descriptor_set_taa);
    VkUtil::update_descriptor_set(vk_context, buffer_descriptor, sizeof(ShaderData), binding++, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptor_set_taa);

    uint32_t descriptor_offset = 0;
    /* noise accumulation*/
#ifdef BENCHMARK
    vkCmdResetQueryPool(commands, query_pool, 0, 2);
    vkCmdWriteTimestamp(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, query_pool, 0);
#endif
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_bmfr);
    vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layou_bmfr, descriptor_offset++, 1, &descriptor_set_bmfr, 0, nullptr);
    vkCmdDispatch(commands, image_width / kernel_x + 1, image_height / kernel_y + 1, 1);
    vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 0, 0, 0, 0);
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_taa);
    vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout_taa, 0, 1, &descriptor_set_taa, 0, nullptr);
    vkCmdDispatch(commands, image_width / kernel_x + 1, image_height / kernel_y + 1, 1);
#ifdef BENCHMARK
    vkCmdWriteTimestamp(commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, query_pool, 1);
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    VkUtil::commit_command_buffer(vk_context, commands);
    vkQueueWaitIdle(vk_context.queue);

#ifdef BENCHMARK
    std::uint64_t timebuild_mesh[2];
    vkGetQueryPoolResults(vk_context.device,
        query_pool,
        0,
        2,
        2 * sizeof(std::uint64_t),
        timebuild_mesh,
        sizeof(std::uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    float build_mesh_time = (timebuild_mesh[1] - timebuild_mesh[0]) * 10 / 1e6f;
    std::cout << "Denoise time: " << build_mesh_time / 10 << "ms\n";
#endif
    //std::vector<float> t(256 * 13);
    //VkUtil::download_data(vk_context, debug_memory, 0, 256 * 10 * sizeof(float), t.data());
    //for (int i = 0; i < 256; ++i) {
    //    for (int j = 0; j < 10; ++j) {
    //        std::cout << t[i * 10 + j] << " | ";
    //    }
    //    std::cout << std::endl;
    //}
    //std::cout << std::endl;
    
    vkFreeCommandBuffers(vk_context.device, vk_context.command_pool, 1, &commands);
}

void Denoiser::get_noise_acc(void* data, uint32_t byte_size)
{
    uint32_t frame_index = frame_number & 1;
    VkUtil::download_image_data(vk_context, image_acc[frame_index], VK_IMAGE_LAYOUT_GENERAL, VK_FORMAT_R32G32B32A32_SFLOAT, image_width, image_height, 1, 0, data, byte_size);
}

void Denoiser::get_data_acc(void* data, uint32_t byte_size)
{
    uint32_t frame_index = frame_number & 1;
    VkUtil::download_image_data(vk_context, image_acc[frame_index], VK_IMAGE_LAYOUT_GENERAL, VK_FORMAT_R32G32B32A32_SFLOAT, image_width, image_height, 1, 1, data, byte_size);
}

void Denoiser::get_denoised(void* data, uint32_t byte_size)
{
    uint32_t frame_index = frame_number & 1;
    VkUtil::download_image_data(vk_context, image_acc[frame_index], VK_IMAGE_LAYOUT_GENERAL, VK_FORMAT_R32G32B32A32_SFLOAT, image_width, image_height, 1, 3, data, byte_size);
}

void Denoiser::create_pipelines()
{
    // -----------------------------------------------------------------------------------------------------------------------
    // bmfr pipeline
    // -----------------------------------------------------------------------------------------------------------------------
    VkShaderModule computeModule = VkUtil::create_shader_module(vk_context, VkUtil::read_byte_file(string_bmfr));

    std::vector<VkDescriptorSetLayoutBinding> bindings;
    VkDescriptorSetLayoutBinding binding = {};
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    binding.binding = 0;								//spp
    binding.descriptorCount = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings.push_back(binding);

    binding.binding = 1;								//accept_bools
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings.push_back(binding);

    binding.binding = 2;								//prevUv
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings.push_back(binding);

    binding.binding = 3;								//current_normals
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings.push_back(binding);

    binding.binding = 4;								//current_positions
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings.push_back(binding);

    binding.binding = 5;								//current_noisy
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings.push_back(binding);

    binding.binding = 6;								//current_spp
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings.push_back(binding);

    binding.binding = 7;								//tmp data
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings.push_back(binding);

    binding.binding = 8;								//previous_normals
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings.push_back(binding);

    binding.binding = 9;								//previous_positions
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings.push_back(binding);

    binding.binding = 10;								//previous_noisy
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings.push_back(binding);

    binding.binding = 11;								//previous_spp
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings.push_back(binding);

    //binding.binding = 11;
    //binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    //bindings.push_back(binding);

    VkUtil::create_descriptor_set_layout(vk_context, bindings, &descriptor_set_layout_bmfr);
    std::vector<VkDescriptorSetLayout> layouts{ descriptor_set_layout_bmfr };

    VkUtil::create_compute_pipeline(vk_context, computeModule, layouts, &pipeline_layou_bmfr, &pipeline_bmfr);

    // -----------------------------------------------------------------------------------------------------------------------
    // taa pipeline
    // -----------------------------------------------------------------------------------------------------------------------
    computeModule = VkUtil::create_shader_module(vk_context, VkUtil::read_byte_file(string_taa));
    bindings.clear();

    binding.binding = 0;								//prev_frame_pixel
    binding.descriptorCount = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings.push_back(binding);

    binding.binding = 1;								//prev_accs
    binding.descriptorCount = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings.push_back(binding);

    binding.binding = 2;								//accs
    binding.descriptorCount = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings.push_back(binding);

    binding.binding = 3;								//prev_frame_pixel
    binding.descriptorCount = 1;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings.push_back(binding);

    VkUtil::create_descriptor_set_layout(vk_context, bindings, &descriptor_set_layout_taa);
    layouts = { descriptor_set_layout_taa };
    VkUtil::create_compute_pipeline(vk_context, computeModule, layouts, &pipeline_layout_taa, &pipeline_taa);
}

void Denoiser::create_images()
{
    VkUtil::create_image_array(vk_context, image_width, image_height, 4, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, &image_acc[0]);
    VkUtil::create_image_array(vk_context, image_width, image_height, 4, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, &image_acc[1]);

    VkUtil::create_image(vk_context, image_width, image_height, VK_FORMAT_R8_UINT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, &image_spp[0]);
    VkUtil::create_image(vk_context, image_width, image_height, VK_FORMAT_R8_UINT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, &image_spp[1]);

    VkUtil::create_image(vk_context, image_width, image_height, VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, &image_prevUV);

    offset_images[0] = 0;
    uint32_t memBits{};
    VkMemoryRequirements memReq;
    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};

    vkGetImageMemoryRequirements(vk_context.device, image_acc[0], &memReq);
    allocInfo.allocationSize += memReq.size;
    memBits |= memReq.memoryTypeBits;

    offset_images[1] = allocInfo.allocationSize;
    vkGetImageMemoryRequirements(vk_context.device, image_acc[1], &memReq);
    allocInfo.allocationSize += memReq.size;
    memBits |= memReq.memoryTypeBits;

    offset_images[2] = allocInfo.allocationSize;
    vkGetImageMemoryRequirements(vk_context.device, image_spp[0], &memReq);
    allocInfo.allocationSize += memReq.size;
    memBits |= memReq.memoryTypeBits;

    offset_images[3] = allocInfo.allocationSize;
    vkGetImageMemoryRequirements(vk_context.device, image_spp[1], &memReq);
    allocInfo.allocationSize += memReq.size;
    memBits |= memReq.memoryTypeBits;

    offset_images[4] = allocInfo.allocationSize;
    vkGetImageMemoryRequirements(vk_context.device, image_prevUV, &memReq);
    allocInfo.allocationSize += memReq.size;
    memBits |= memReq.memoryTypeBits;

    allocInfo.memoryTypeIndex = VkUtil::find_memory_type(vk_context, memBits, 0);
    VkResult err = vkAllocateMemory(vk_context.device, &allocInfo, vk_context.allocator, &memory_images);
    VkUtil::check_vk_result(err);

    err = vkBindImageMemory(vk_context.device, image_acc[0], memory_images, offset_images[0]);  VkUtil::check_vk_result(err);
    err = vkBindImageMemory(vk_context.device, image_acc[1], memory_images, offset_images[1]);  VkUtil::check_vk_result(err);
    err = vkBindImageMemory(vk_context.device, image_spp[0], memory_images, offset_images[2]);  VkUtil::check_vk_result(err);
    err = vkBindImageMemory(vk_context.device, image_spp[1], memory_images, offset_images[3]);  VkUtil::check_vk_result(err);
    err = vkBindImageMemory(vk_context.device, image_prevUV, memory_images, offset_images[4]);  VkUtil::check_vk_result(err);
   
    VkUtil::create_image_view_array(vk_context, image_acc[0], VK_FORMAT_R32G32B32A32_SFLOAT, 1, 4, VK_IMAGE_ASPECT_COLOR_BIT, &imageView_acc[0]);
    VkUtil::create_image_view_array(vk_context, image_acc[1], VK_FORMAT_R32G32B32A32_SFLOAT, 1, 4, VK_IMAGE_ASPECT_COLOR_BIT, &imageView_acc[1]);
    VkUtil::create_image_view(vk_context, image_spp[0], VK_FORMAT_R8_UINT, 1, VK_IMAGE_ASPECT_COLOR_BIT, &imageView_spp[0]);
    VkUtil::create_image_view(vk_context, image_spp[1], VK_FORMAT_R8_UINT, 1, VK_IMAGE_ASPECT_COLOR_BIT, &imageView_spp[1]);
    VkUtil::create_image_view(vk_context, image_prevUV, VK_FORMAT_R16G16_SFLOAT, 1, VK_IMAGE_ASPECT_COLOR_BIT, &imageView_prevUV);

    VkCommandBuffer commands;
    VkUtil::create_command_buffer(vk_context, &commands);
    VkUtil::transition_image_layout(commands, image_acc[0], VK_FORMAT_R32G32B32A32_SFLOAT, 4, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    VkUtil::transition_image_layout(commands, image_acc[1], VK_FORMAT_R32G32B32A32_SFLOAT, 4, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    VkUtil::transition_image_layout(commands, image_spp[0], VK_FORMAT_R8_UINT, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    VkUtil::transition_image_layout(commands, image_spp[1], VK_FORMAT_R8_UINT, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    VkUtil::transition_image_layout(commands, image_prevUV, VK_FORMAT_R16G16_SFLOAT, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    VkUtil::commit_command_buffer(vk_context, commands);
    vkQueueWaitIdle(vk_context.queue);
    vkFreeCommandBuffers(vk_context.device, vk_context.command_pool, 1, &commands);
}

void Denoiser::create_buffers()
{
    VkUtil::create_buffer(vk_context, sizeof(ShaderData),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &buffer_descriptor);

    uint32_t memBits{};
    VkMemoryRequirements memReq;
    VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };

    vkGetBufferMemoryRequirements(vk_context.device, buffer_descriptor, &memReq);
    allocInfo.allocationSize += (memReq.size + memReq.alignment - 1) / memReq.alignment * memReq.alignment;     // aligning the memory to the memory alignment of the graphics card
    memBits |= memReq.memoryTypeBits;

    allocInfo.memoryTypeIndex = VkUtil::find_memory_type(vk_context, memBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkResult err = vkAllocateMemory(vk_context.device, &allocInfo, vk_context.allocator, &memory_buffer);
    VkUtil::check_vk_result(err);

    err = vkBindBufferMemory(vk_context.device, buffer_descriptor, memory_buffer, 0);   VkUtil::check_vk_result(err);

    VkUtil::create_buffer(vk_context, 256 * 13 * sizeof(float), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, &debug_buffer);
    vkGetBufferMemoryRequirements(vk_context.device, debug_buffer, &memReq);
    allocInfo.allocationSize = memReq.size;
    memBits = memReq.memoryTypeBits;
    allocInfo.memoryTypeIndex = VkUtil::find_memory_type(vk_context, memBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    err = vkAllocateMemory(vk_context.device, &allocInfo, vk_context.allocator, &debug_memory); VkUtil::check_vk_result(err);
    err = vkBindBufferMemory(vk_context.device, debug_buffer, debug_memory, 0);
}

void Denoiser::create_sampler()
{
    VkUtil::create_image_sampler(vk_context, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_FILTER_LINEAR, 0, 1, &sampler_image);;
}

void Denoiser::create_descriptor_set()
{
    std::vector<VkDescriptorSetLayout> layouts{ descriptor_set_layout_bmfr, descriptor_set_layout_taa };
    VkUtil::create_descriptor_sets(vk_context, layouts, &descriptor_set_bmfr);
}

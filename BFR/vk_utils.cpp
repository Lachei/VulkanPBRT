#include "vk_utils.h"

uint32_t VkUtil::find_memory_type(Vk_Context& context, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(context.physical_device, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    //safety call to see whther a valid type Index was found
#ifdef _DEBUG
    std::cerr << "The memory type which is needed is not available!" << std::endl;
    exit(-1);
#endif
    return -1;
}

void VkUtil::check_vk_result(VkResult err)
{
	if (err == 0)
		return;
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
	if (err < 0)
		abort();
}

#ifdef VULKAN_DEBUG_REPORT
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData)
{
	(void)flags; (void)object; (void)location; (void)messageCode; (void)pUserData; (void)pLayerPrefix; // Unused arguments
	fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
	return VK_FALSE;
}
#endif // VULKAN_DEBUG_REPORT

void VkUtil::create_vk_context(std::vector<const char*>& extensions, Vk_Context& context)
{
	VkResult err;

	// Create Vulkan Instance
	{
        VkApplicationInfo app_info{};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.apiVersion = VK_API_VERSION_1_2;

		VkInstanceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.enabledExtensionCount = extensions.size();
		create_info.ppEnabledExtensionNames = extensions.data();
        create_info.pApplicationInfo = &app_info;

#ifdef VULKAN_DEBUG_REPORT
		// Enabling multiple validation layers grouped as LunarG standard validation
		const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
		create_info.enabledLayerCount = 1;
		create_info.ppEnabledLayerNames = layers;

		// Enable debug report extension (we need additional storage, so we duplicate the user array to add our new extension to it)
		const char** extensions_ext = (const char**)malloc(sizeof(const char*) * (extensions.size() + 1));
		int c = 0;
		for (const char* ch : extensions) extensions_ext[c++] = ch;
		extensions_ext[extensions.size()] = "VK_EXT_debug_report";
		create_info.enabledExtensionCount = extensions.size() + 1;
		create_info.ppEnabledExtensionNames = extensions_ext;

		// Create Vulkan Instance
		err = vkCreateInstance(&create_info, context.allocator, &context.instance);
		check_vk_result(err);
		free(extensions_ext);

		// Get the function pointer (required for any extensions)
		auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(context.instance, "vkCreateDebugReportCallbackEXT");
		assert(vkCreateDebugReportCallbackEXT != NULL);

		// Setup the debug report callback
		VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
		debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		debug_report_ci.pfnCallback = debug_report;
		debug_report_ci.pUserData = NULL;
		err = vkCreateDebugReportCallbackEXT(context.instance, &debug_report_ci, context.allocator, &context.debug_report);
		check_vk_result(err);
#else
		// Create Vulkan Instance without any debug feature
		err = vkCreateInstance(&create_info, context.allocator, &context.instance);
		check_vk_result(err);
#endif
	}

	// Select GPU
	{
		uint32_t gpu_count;
		err = vkEnumeratePhysicalDevices(context.instance, &gpu_count, NULL);
		check_vk_result(err);
		assert(gpu_count > 0);

		VkPhysicalDevice* gpus = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * gpu_count);
		err = vkEnumeratePhysicalDevices(context.instance, &gpu_count, gpus);
		check_vk_result(err);

		// If a number >1 of GPUs got reported, you should find the best fit GPU for your purpose
		// e.g. VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU if available, or with the greatest memory available, etc.
		// for sake of simplicity we'll just take the first one, assuming it has a graphics queue family.
		context.physical_device = gpus[0];
		free(gpus);
	}

	// Select graphics queue family
	{
		uint32_t count;
		vkGetPhysicalDeviceQueueFamilyProperties(context.physical_device, &count, NULL);
		VkQueueFamilyProperties* queues = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * count);
		vkGetPhysicalDeviceQueueFamilyProperties(context.physical_device, &count, queues);
		assert(context.queue_type != VK_QUEUE_FLAG_BITS_MAX_ENUM);
		for (uint32_t i = 0; i < count; i++)
			if (queues[i].queueFlags & context.queue_type)
			{
				context.queue_family = i;
				break;
			}
		free(queues);
		assert(context.queue_family != (uint32_t)-1);
	}

	// Create Logical Device (with 1 queue)
	{
		std::vector<const char*> device_extensions{};
		const float queue_priority[] = { 1.0f };
        VkPhysicalDeviceFeatures deviceFeatures{};
		VkDeviceQueueCreateInfo queue_info[1] = {};
		queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_info[0].queueFamilyIndex = context.queue_family;
		queue_info[0].queueCount = 1;
		queue_info[0].pQueuePriorities = queue_priority;
		VkDeviceCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
		create_info.pQueueCreateInfos = queue_info;
		create_info.enabledExtensionCount = device_extensions.size();
		create_info.ppEnabledExtensionNames = device_extensions.data();
		err = vkCreateDevice(context.physical_device, &create_info, context.allocator, &context.device);
		check_vk_result(err);
		vkGetDeviceQueue(context.device, context.queue_family, 0, &context.queue);
	}

	// Create Descriptor Pool
	{
		VkDescriptorPoolSize pool_sizes[] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 1000 * sizeof(pool_sizes)/sizeof(pool_sizes[0]);
		pool_info.poolSizeCount = (uint32_t)sizeof(pool_sizes) / sizeof(pool_sizes[0]);
		pool_info.pPoolSizes = pool_sizes;
		err = vkCreateDescriptorPool(context.device, &pool_info, context.allocator, &context.descriptor_pool);
		check_vk_result(err);
	}

    // Create Command Pool
    {
        VkCommandPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = context.queue_family;

        err = vkCreateCommandPool(context.device, &poolInfo, context.allocator, &context.command_pool);
        check_vk_result(err);
    }
}

void VkUtil::destroy_vk_context(Vk_Context& context)
{
    vkQueueWaitIdle(context.queue);
    vkDestroyDescriptorPool(context.device, context.descriptor_pool, context.allocator);
    vkDestroyCommandPool(context.device, context.command_pool, context.allocator);

#ifdef VULKAN_DEBUG_REPORT
    // Remove the debug report callback
    auto vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(context.instance, "vkDestroyDebugReportCallbackEXT");
    vkDestroyDebugReportCallbackEXT(context.instance, context.debug_report, context.allocator);
#endif // VULKAN_DEBUG_REPORT

    vkDestroyDevice(context.device, context.allocator);
    vkDestroyInstance(context.instance, context.allocator);
}

void VkUtil::create_compute_pipeline(Vk_Context& context, VkShaderModule& shaderModule, std::vector<VkDescriptorSetLayout> descriptorLayouts, VkPipelineLayout* pipelineLayout, VkPipeline* pipeline)
{
    VkResult err;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = descriptorLayouts.size();
    pipelineLayoutInfo.pSetLayouts = descriptorLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    err = vkCreatePipelineLayout(context.device, &pipelineLayoutInfo, context.allocator, pipelineLayout);
    check_vk_result(err);

    VkPipelineShaderStageCreateInfo shaderInfo = {};
    shaderInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderInfo.module = shaderModule;
    shaderInfo.pName = "main";

    VkComputePipelineCreateInfo pipeInfo = {};
    pipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeInfo.layout = *pipelineLayout;
    pipeInfo.stage = shaderInfo;

    err = vkCreateComputePipelines(context.device, VK_NULL_HANDLE, 1, &pipeInfo, context.allocator, pipeline);
    check_vk_result(err);

    vkDestroyShaderModule(context.device, shaderModule, context.allocator);
}

void VkUtil::create_descriptor_set_layout(Vk_Context& context, const std::vector<VkDescriptorSetLayoutBinding>& bindings, VkDescriptorSetLayout* descriptorSetLayout)
{
    VkResult err;

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    err = vkCreateDescriptorSetLayout(context.device, &layoutInfo, context.allocator, descriptorSetLayout);
    check_vk_result(err);
}

void VkUtil::create_descriptor_setLayout_partially_bound(Vk_Context& context, const std::vector<VkDescriptorSetLayoutBinding>& bindings, const std::vector<bool>& enableValidation, VkDescriptorSetLayout* descriptorSetLayout)
{
    VkResult err;

    std::vector<VkDescriptorBindingFlagsEXT> bindFlag(bindings.size(), VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT);
    for (int i = 0; i < enableValidation.size(); ++i) {
        bindFlag[i] = (enableValidation[i]) ? 0 : VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT;
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfoEXT extendedInfo = {};
    extendedInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;
    extendedInfo.pNext = nullptr;
    extendedInfo.bindingCount = bindings.size();
    extendedInfo.pBindingFlags = bindFlag.data();

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    layoutInfo.pNext = &extendedInfo;

    err = vkCreateDescriptorSetLayout(context.device, &layoutInfo, context.allocator, descriptorSetLayout);
    check_vk_result(err);
}

void VkUtil::create_descriptor_sets(Vk_Context& context, const std::vector<VkDescriptorSetLayout>& layouts, VkDescriptorSet* descriptorSetArray)
{
    VkResult err;

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = context.descriptor_pool;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    err = vkAllocateDescriptorSets(context.device, &allocInfo, descriptorSetArray);
    check_vk_result(err);
}

void VkUtil::update_descriptor_set(Vk_Context& context, VkBuffer buffer, uint32_t size, uint32_t binding, VkDescriptorType descriptorType, VkDescriptorSet descriptorSet)
{
    VkDescriptorBufferInfo desBufferInfo = {};
    desBufferInfo.buffer = buffer;
    desBufferInfo.offset = 0;
    desBufferInfo.range = size;

    VkWriteDescriptorSet descriptorWrite = {};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = binding;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = descriptorType;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &desBufferInfo;

    vkUpdateDescriptorSets(context.device, 1, &descriptorWrite, 0, nullptr);
}

void VkUtil::update_image_descriptor_set(Vk_Context& context, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout, VkDescriptorType imageType, uint32_t binding, VkDescriptorSet descriptorSet)
{
    VkDescriptorImageInfo desc_image[1] = {};
    desc_image[0].sampler = sampler;
    desc_image[0].imageView = imageView;
    desc_image[0].imageLayout = imageLayout;
    VkWriteDescriptorSet write_desc[1] = {};
    write_desc[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_desc[0].dstSet = descriptorSet;
    write_desc[0].descriptorCount = 1;
    write_desc[0].descriptorType = imageType;
    write_desc[0].pImageInfo = desc_image;
    write_desc[0].dstBinding = binding;
    vkUpdateDescriptorSets(context.device, 1, write_desc, 0, NULL);
}

void VkUtil::create_image(Vk_Context& context, uint32_t width, uint32_t height, VkFormat imageFormat, VkImageUsageFlags usageFlags, VkImage* image)
{
    VkResult err;

    //creating the VkImage for the PcPlot
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = imageFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usageFlags;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    err = vkCreateImage(context.device, &imageInfo, context.allocator, image);
    check_vk_result(err);
}

void VkUtil::create_image_array(Vk_Context& context, uint32_t width, uint32_t height, uint32_t array_count, VkFormat imageFormat, VkImageUsageFlags usageFlags, VkImage* image)
{
    VkResult err;

    //creating the VkImage for the PcPlot
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = array_count;
    imageInfo.format = imageFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usageFlags;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    err = vkCreateImage(context.device, &imageInfo, context.allocator, image);
    check_vk_result(err);
}

void VkUtil::create_image_view_array(Vk_Context& context, VkImage image, VkFormat imageFormat, uint32_t mipLevelCount, uint32_t array_count, VkImageAspectFlags aspectMask, VkImageView* imageView)
{
    VkResult err;

    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    createInfo.format = imageFormat;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = aspectMask;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = mipLevelCount;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = array_count;

    err = vkCreateImageView(context.device, &createInfo, context.allocator, imageView);
    check_vk_result(err);
}

void VkUtil::create_buffer(Vk_Context& context, uint32_t size, VkBufferUsageFlags usage, VkBuffer* buffer)
{
    VkResult err;

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    err = vkCreateBuffer(context.device, &bufferInfo, context.allocator, buffer);
    check_vk_result(err);
}

void VkUtil::create_image_sampler(Vk_Context& context, VkSamplerAddressMode adressMode, VkFilter filter, uint16_t maxAnisotropy, uint16_t mipLevels, VkSampler* sampler)
{
    VkResult err;

    VkSamplerCreateInfo samplerInfo = {};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = filter;
    samplerInfo.minFilter = filter;
    samplerInfo.addressModeU = adressMode;
    samplerInfo.addressModeV = adressMode;
    samplerInfo.addressModeW = adressMode;
    samplerInfo.anisotropyEnable = maxAnisotropy > 0 ? VK_TRUE : VK_FALSE;
    samplerInfo.maxAnisotropy = maxAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = (float)(mipLevels - 1);

    err = vkCreateSampler(context.device, &samplerInfo, context.allocator, sampler);
    check_vk_result(err);
}

void VkUtil::create_command_buffer(Vk_Context& context, VkCommandBuffer* commandBuffer)
{
    VkResult err;

    VkCommandBufferAllocateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    bufferInfo.commandPool = context.command_pool;
    bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    bufferInfo.commandBufferCount = 1;

    err = vkAllocateCommandBuffers(context.device, &bufferInfo, commandBuffer);
    check_vk_result(err);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    beginInfo.pInheritanceInfo = nullptr;

    err = vkBeginCommandBuffer(*commandBuffer, &beginInfo);
    check_vk_result(err);
}

void VkUtil::commit_command_buffer(Vk_Context& context, VkCommandBuffer commandBuffer)
{
    VkResult err;

    err = vkEndCommandBuffer(commandBuffer);
    check_vk_result(err);

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.signalSemaphoreCount = 0;
    submitInfo.waitSemaphoreCount = 0;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    err = vkQueueSubmit(context.queue, 1, &submitInfo, VK_NULL_HANDLE);
    check_vk_result(err);
}

void VkUtil::copy_buffer_to_image(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImageLayout layout, VkImage image, uint32_t width, uint32_t height, uint32_t depth, uint32_t array_layer)
{
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = array_layer;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = {
        width,
        height,
        depth
    };

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, layout, 1, &region);
}

void VkUtil::copy_image_to_buffer(VkCommandBuffer commandBuffer, VkBuffer buffer, VkImageLayout layout, VkImage image, uint32_t width, uint32_t height, uint32_t depth, uint32_t array_layer)
{
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = array_layer;
    region.imageSubresource.layerCount = 1;

    region.imageOffset = { 0, 0, 0 };
    region.imageExtent = {
        width,
        height,
        depth
    };

    vkCmdCopyImageToBuffer(commandBuffer, image, layout, buffer, 1, &region);
}

void VkUtil::upload_data(Vk_Context& context, VkDeviceMemory memory, uint32_t offset, uint32_t byteSize, void* data)
{
    void* d;
    vkMapMemory(context.device, memory, offset, byteSize, 0, &d);
    memcpy(d, data, byteSize);
    vkUnmapMemory(context.device, memory);
}

void VkUtil::download_data(Vk_Context& context, VkDeviceMemory memory, uint32_t offset, uint32_t byteSize, void* data)
{
    void* d;
    vkMapMemory(context.device, memory, offset, byteSize, 0, &d);
    memcpy(data, d, byteSize);
    vkUnmapMemory(context.device, memory);
}

void VkUtil::upload_image_data(Vk_Context& context, VkImage image, VkImageLayout imageLayout, VkFormat imageFormat, uint32_t x, uint32_t y, uint32_t z, uint32_t array_layer, void* data, uint32_t byteSize)
{
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    create_buffer(context, byteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &stagingBuffer);
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(context.device, stagingBuffer, &memReq);
    VkMemoryAllocateInfo memAlloc{};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAlloc.allocationSize = memReq.size;
    memAlloc.memoryTypeIndex = find_memory_type(context, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(context.device, &memAlloc, nullptr, &stagingMemory);
    vkBindBufferMemory(context.device, stagingBuffer, stagingMemory, 0);

    upload_data(context, stagingMemory, 0, byteSize, data);
    VkCommandBuffer commands;
    create_command_buffer(context, &commands);
    transition_image_layout(commands, image, imageFormat, array_layer + 1, imageLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(commands, stagingBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image, x, y, z, array_layer);
    transition_image_layout(commands, image, imageFormat, array_layer + 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, imageLayout);
    commit_command_buffer(context, commands);
    check_vk_result(vkQueueWaitIdle(context.queue));

    vkDestroyBuffer(context.device, stagingBuffer, context.allocator);
    vkFreeMemory(context.device, stagingMemory, context.allocator);
    vkFreeCommandBuffers(context.device, context.command_pool, 1, &commands);
}

void VkUtil::download_image_data(Vk_Context& context, VkImage image, VkImageLayout layout, VkFormat imageFormat, uint32_t x, uint32_t y, uint32_t z, uint32_t array_layer, void* data, uint32_t byteSize)
{
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    create_buffer(context, byteSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT, &stagingBuffer);
    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(context.device, stagingBuffer, &memReq);
    VkMemoryAllocateInfo memAlloc{};
    memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memAlloc.allocationSize = memReq.size;
    memAlloc.memoryTypeIndex = find_memory_type(context, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    vkAllocateMemory(context.device, &memAlloc, nullptr, &stagingMemory);
    vkBindBufferMemory(context.device, stagingBuffer, stagingMemory, 0);

    VkCommandBuffer commands;
    create_command_buffer(context, &commands);
    transition_image_layout(commands, image, imageFormat, array_layer + 1, layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    copy_image_to_buffer(commands, stagingBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, x, y, z, array_layer);
    transition_image_layout(commands, image, imageFormat, array_layer + 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, layout);
    commit_command_buffer(context, commands);
    check_vk_result(vkQueueWaitIdle(context.queue));
    download_data(context, stagingMemory, 0, byteSize, data);

    vkDestroyBuffer(context.device, stagingBuffer, context.allocator);
    vkFreeMemory(context.device, stagingMemory, context.allocator);
    vkFreeCommandBuffers(context.device, context.command_pool, 1, &commands);
}

void VkUtil::transition_image_layout(VkCommandBuffer commandBuffer, VkImage image, VkFormat format, uint32_t layer_count, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layer_count;

    VkPipelineStageFlags sourceStage = 0;
    VkPipelineStageFlags destinationStage = 0;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        destinationStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        destinationStage = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    }
    else {
        std::cerr << "Unknown Layout transition from " << oldLayout << " to " << newLayout << "!" << std::endl;
        exit(-1);
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VkUtil::create_image_view(Vk_Context& context, VkImage image, VkFormat imageFormat, uint32_t mipLevelCount, VkImageAspectFlags aspectMask, VkImageView* imageView)
{
    VkResult err;

    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = imageFormat;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = aspectMask;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = mipLevelCount;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;

    err = vkCreateImageView(context.device, &createInfo, context.allocator, imageView);
    check_vk_result(err);
}

VkShaderModule VkUtil::create_shader_module(Vk_Context& context, const std::vector<char>& byteArr)
{
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pCode = reinterpret_cast<const uint32_t*>(byteArr.data());
    createInfo.codeSize = byteArr.size();

    VkShaderModule shaderModule;
    VkResult err = vkCreateShaderModule(context.device, &createInfo, context.allocator, &shaderModule);
    check_vk_result(err);

    return shaderModule;
}

std::vector<char> VkUtil::read_byte_file(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "failed to open file " << filename << "!" << std::endl;
        exit(-1);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();

    return buffer;
}

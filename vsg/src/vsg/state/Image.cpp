/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/io/Options.h>
#include <vsg/state/Image.h>
#include <vsg/vk/Context.h>

using namespace vsg;

void Image::VulkanData::release()
{
    if (image)
    {
        vkDestroyImage(*device, image, device->getAllocationCallbacks());
        image = VK_NULL_HANDLE;
    }

    if (deviceMemory)
    {
        deviceMemory->release(memoryOffset, size);
        deviceMemory = {};
    }
}

Image::Image(ref_ptr<Data> in_data) :
    data(in_data)
{
    if (data)
    {
        auto layout = data->getLayout();
        auto mipmapOffsets = data->computeMipmapOffsets();
        auto dimensions = data->dimensions();

        uint32_t width = data->width() * layout.blockWidth;
        uint32_t height = data->height() * layout.blockHeight;
        uint32_t depth = data->depth() * layout.blockDepth;

        switch (layout.imageViewType)
        {
        case (VK_IMAGE_VIEW_TYPE_1D):
            imageType = VK_IMAGE_TYPE_1D;
            arrayLayers = 1;
            break;
        case (VK_IMAGE_VIEW_TYPE_2D):
            imageType = VK_IMAGE_TYPE_2D;
            arrayLayers = 1;
            break;
        case (VK_IMAGE_VIEW_TYPE_3D):
            imageType = VK_IMAGE_TYPE_3D;
            arrayLayers = 1;
            break;
        case (VK_IMAGE_VIEW_TYPE_CUBE):
            imageType = VK_IMAGE_TYPE_2D;
            flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            arrayLayers = depth;
            depth = 1;
            break;
        case (VK_IMAGE_VIEW_TYPE_1D_ARRAY):
            imageType = VK_IMAGE_TYPE_1D;
            arrayLayers = height * depth;
            height = 1;
            depth = 1;
            /* flags = VK_IMAGE_CREATE_1D_ARRAY_COMPATIBLE_BIT; // comment out as Vulkan headers don't yet provide this. */
            break;
        case (VK_IMAGE_VIEW_TYPE_2D_ARRAY):
            imageType = VK_IMAGE_TYPE_2D;
            flags = VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
            arrayLayers = depth;
            depth = 1;
            break;
        case (VK_IMAGE_VIEW_TYPE_CUBE_ARRAY):
            imageType = VK_IMAGE_TYPE_2D;
            flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            arrayLayers = depth;
            depth = 1;
            break;
        default:
            imageType = dimensions >= 3 ? VK_IMAGE_TYPE_3D : (dimensions == 2 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D);
            arrayLayers = 1;
            break;
        }

        format = layout.format;
        mipLevels = static_cast<uint32_t>(mipmapOffsets.size());
        extent = VkExtent3D{width, height, depth};

        // remap RGB to RGBA
        if (format >= VK_FORMAT_R8G8B8_UNORM && format <= VK_FORMAT_B8G8R8_SRGB)
            format = static_cast<VkFormat>(format + 14);
        else if (format >= VK_FORMAT_R16G16B16_UNORM && format <= VK_FORMAT_R16G16B16_SFLOAT)
            format = static_cast<VkFormat>(format + 7);
        else if (format >= VK_FORMAT_R32G32B32_UINT && format <= VK_FORMAT_R32G32B32_SFLOAT)
            format = static_cast<VkFormat>(format + 3);
    }
}

Image::Image(VkImage image, Device* device)
{
    VulkanData& vd = _vulkanData[device->deviceID];
    vd.image = image;
    vd.device = device;
}

Image::~Image()
{
    for (auto& vd : _vulkanData) vd.release();
}

VkResult Image::bind(DeviceMemory* deviceMemory, VkDeviceSize memoryOffset)
{
    VulkanData& vd = _vulkanData[deviceMemory->getDevice()->deviceID];

    VkResult result = vkBindImageMemory(*vd.device, vd.image, *deviceMemory, memoryOffset);
    if (result == VK_SUCCESS)
    {
        vd.deviceMemory = deviceMemory;
        vd.memoryOffset = memoryOffset;
    }
    return result;
}

VkMemoryRequirements Image::getMemoryRequirements(uint32_t deviceID) const
{
    const VulkanData& vd = _vulkanData[deviceID];

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(*vd.device, vd.image, &memRequirements);
    return memRequirements;
}

void Image::compile(Device* device)
{
    auto& vd = _vulkanData[device->deviceID];
    if (vd.image != VK_NULL_HANDLE) return;

    VkImageCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = flags;
    info.imageType = imageType;
    info.format = format;
    info.extent = extent;
    info.mipLevels = mipLevels;
    info.arrayLayers = arrayLayers;
    info.samples = samples;
    info.tiling = tiling;
    info.usage = usage;
    info.sharingMode = sharingMode;
    info.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
    info.pQueueFamilyIndices = queueFamilyIndices.data();
    info.initialLayout = initialLayout;

    vd.device = device;

    if (VkResult result = vkCreateImage(*vd.device, &info, vd.device->getAllocationCallbacks(), &vd.image); result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create vkImage.", result};
    }
}

void Image::compile(Context& context)
{
    auto& vd = _vulkanData[context.deviceID];
    if (vd.image != VK_NULL_HANDLE) return;

    compile(context.device);

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(*vd.device, vd.image, &memRequirements);

    auto [deviceMemory, offset] = context.deviceMemoryBufferPools->reserveMemory(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (!deviceMemory)
    {
        throw Exception{"Error: allocate memory to reserve slot.", VK_ERROR_OUT_OF_DEVICE_MEMORY};
    }

    bind(deviceMemory, offset);
}

#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/state/ImageView.h>
#include <vsg/vk/Fence.h>
#include <vsg/vk/Surface.h>

namespace vsg
{
    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    extern VSG_DECLSPEC SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
    extern VSG_DECLSPEC VkSurfaceFormatKHR selectSwapSurfaceFormat(const SwapChainSupportDetails& details, VkSurfaceFormatKHR preferredSurfaceFormat = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR});
    extern VSG_DECLSPEC VkExtent2D selectSwapExtent(const SwapChainSupportDetails& details, uint32_t width, uint32_t height);
    extern VSG_DECLSPEC VkPresentModeKHR selectSwapPresentMode(const SwapChainSupportDetails& details, VkPresentModeKHR preferredPresentMode = VK_PRESENT_MODE_MAILBOX_KHR);

    struct SwapchainPreferences
    {
        uint32_t imageCount = 3; // default to triple buffering
        VkSurfaceFormatKHR surfaceFormat = {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    };

    class SwapchainImage : public Inherit<Image, SwapchainImage>
    {
    public:
        SwapchainImage(VkImage image, Device* device);

    protected:
        virtual ~SwapchainImage();
    };
    VSG_type_name(vsg::SwapchainImage);

    class VSG_DECLSPEC Swapchain : public Inherit<Object, Swapchain>
    {
    public:
        Swapchain(PhysicalDevice* physicalDevice, Device* device, Surface* surface, uint32_t width, uint32_t height, SwapchainPreferences& preferences);

        operator VkSwapchainKHR() const { return _swapchain; }

        VkFormat getImageFormat() const { return _format; }

        const VkExtent2D& getExtent() const { return _extent; }

        ImageViews& getImageViews() { return _imageViews; }
        const ImageViews& getImageViews() const { return _imageViews; }

        /// call vkAcquireNextImageKHR
        VkResult acquireNextImage(uint64_t timeout, ref_ptr<Semaphore> semaphore, ref_ptr<Fence> fence, uint32_t& imageIndex);

    protected:
        virtual ~Swapchain();

        vsg::ref_ptr<Device> _device;
        vsg::ref_ptr<Surface> _surface;
        VkSwapchainKHR _swapchain;
        VkFormat _format;
        VkExtent2D _extent;
        ImageViews _imageViews;
    };
    VSG_type_name(vsg::Swapchain);

} // namespace vsg

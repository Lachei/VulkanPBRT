/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/Options.h>
#include <vsg/vk/PhysicalDevice.h>

using namespace vsg;

PhysicalDevice::PhysicalDevice(Instance* instance, VkPhysicalDevice device) :
    _device(device),
    _instance(instance)
{
    vkGetPhysicalDeviceFeatures(_device, &_features);

    vkGetPhysicalDeviceProperties(_device, &_properties);

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_device, &queueFamilyCount, nullptr);

    _queueFamiles.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(_device, &queueFamilyCount, _queueFamiles.data());
}

PhysicalDevice::~PhysicalDevice()
{
}

int PhysicalDevice::getQueueFamily(VkQueueFlags queueFlags) const
{
    int bestFamily = -1;

    for (int i = 0; i < static_cast<int>(_queueFamiles.size()); ++i)
    {
        const auto& queueFamily = _queueFamiles[i];
        if ((queueFamily.queueFlags & queueFlags) == queueFlags)
        {
            // check for perfect match
            if (queueFamily.queueFlags == queueFlags)
            {
                return i;
            }

            if (bestFamily < 0) bestFamily = i;
        }
    }

    return bestFamily;
}

std::pair<int, int> PhysicalDevice::getQueueFamily(VkQueueFlags queueFlags, Surface* surface) const
{
    int queueFamily = -1;
    int presentFamily = -1;

    for (int i = 0; i < static_cast<int>(_queueFamiles.size()); ++i)
    {
        const auto& family = _queueFamiles[i];

        bool queueMatched = (family.queueFlags & queueFlags) == queueFlags;

        VkBool32 presentSupported = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(_device, i, *surface, &presentSupported);

        if (queueMatched && presentSupported)
        {
            return {i, i};
        }

        if (queueMatched) queueFamily = i;
        if (presentSupported) presentFamily = i;
    }

    return {queueFamily, presentFamily};
}

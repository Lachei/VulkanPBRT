#pragma once

#include <vsg/core/Object.h>
#include <vsg/vk/Device.h>
#include <vsg/core/ref_ptr.h>
#include <vulkan/vulkan.h>

#include <string>
#include <vector>

namespace vsg
{
    using PipelineCaches = std::vector<VkPipelineCache>;
    class VSG_DECLSPEC PipelineCache: public Object
    {
    public:
        PipelineCache(ref_ptr<Device> device,const std::string& filename = "");

        PipelineCaches  pipelineCaches;
    protected:
        ~PipelineCache();
        ref_ptr<Device> _device;
    };
}
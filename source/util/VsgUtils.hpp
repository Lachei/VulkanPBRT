#pragma once

#include <vsg/all.h>

namespace vkpbrt{
     vsg::ref_ptr<vsg::RenderPass> createNonClearRenderPass(VkFormat colorFormat, VkFormat depthFormat, vsg::Device* device);
}
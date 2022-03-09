#pragma once

#include <vsg/all.h>

namespace vkpbrt
{
vsg::ref_ptr<vsg::RenderPass> create_non_clear_render_pass(
    VkFormat color_format, VkFormat depth_format, vsg::Device* device);
}
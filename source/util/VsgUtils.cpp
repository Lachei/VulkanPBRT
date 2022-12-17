#include <util/VsgUtils.hpp>

namespace vkpbrt
{
vsg::ref_ptr<vsg::RenderPass> create_non_clear_render_pass(
    VkFormat color_format, VkFormat depth_format, vsg::Device* device)
{
    vsg::AttachmentDescription color_attachment = vsg::defaultColorAttachment(color_format);
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vsg::AttachmentDescription depth_attachment = vsg::defaultDepthAttachment(depth_format);
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    vsg::RenderPass::Attachments attachments{color_attachment, depth_attachment};

    vsg::AttachmentReference color_attachment_ref = {};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    vsg::AttachmentReference depth_attachment_ref = {};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    vsg::SubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachments.emplace_back(color_attachment_ref);
    subpass.depthStencilAttachments.emplace_back(depth_attachment_ref);

    vsg::RenderPass::Subpasses subpasses{subpass};

    // image layout transition
    vsg::SubpassDependency color_dependency = {};
    color_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    color_dependency.dstSubpass = 0;
    color_dependency.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    color_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    color_dependency.srcAccessMask = 0;
    color_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    color_dependency.dependencyFlags = 0;

    // depth buffer is shared between swap chain images
    vsg::SubpassDependency depth_dependency = {};
    depth_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    depth_dependency.dstSubpass = 0;
    depth_dependency.srcStageMask
        = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depth_dependency.dstStageMask
        = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    depth_dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depth_dependency.dstAccessMask
        = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depth_dependency.dependencyFlags = 0;

    vsg::RenderPass::Dependencies dependencies{color_dependency, depth_dependency};

    return vsg::RenderPass::create(device, attachments, subpasses, dependencies);
}
}  // namespace vkpbrt
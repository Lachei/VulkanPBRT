#include <renderModules/GBufferRasterizer.hpp>

GBufferRasterizer::GBufferRasterizer(vsg::Device* device, uint32_t width, uint32_t height, bool doubleSided, bool blend) :
    width(width), height(height), device(device), gBuffer(GBuffer::create(width, height)), doubleSided(doubleSided), blend(blend)
{
    setupGraphicsPipeline();
}
void GBufferRasterizer::compile(vsg::Context& context)
{
    gBuffer->compile(context);
}
void GBufferRasterizer::updateImageLayouts(vsg::Context& context)
{
    gBuffer->updateImageLayouts(context);
}
void GBufferRasterizer::setupGraphicsPipeline()
{
    //loading shaders and getting the descriptor set layout from the shader
    std::string vertexPath = "shaders/rasterizer.vert.spv";
    std::string fragmentPath = "shaders/rasterizer.frag.spv";

    auto vertexShader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertexPath);
    auto fragmentShader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragmentPath);
    vsg::BindingMap bindingMap = vsg::ShaderStage::mergeBindingMaps({
        vertexShader->getDescriptorSetLayoutBindingsMap(),
        fragmentShader->getDescriptorSetLayoutBindingsMap()
    });

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(bindingMap.begin()->second.bindings);

    // code mostly copied from vsg::src::vsgXchange::assimp::assimp.cpp
    vsg::PushConstantRanges pushConstantRanges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128}
        // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's DispatchTraversal
    };

    vsg::VertexInputState::Bindings vertexBindingsDescriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // normal data
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX} // texcoord data
    };

    vsg::VertexInputState::Attributes vertexAttributeDescriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // normal data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT, 0} // texcoord data
    };

    auto rasterState = vsg::RasterizationState::create();
    rasterState->cullMode = doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

    auto colorBlendState = vsg::ColorBlendState::create();
    colorBlendState->attachments = vsg::ColorBlendState::ColorBlendAttachments{
        {
            blend, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_SRC_ALPHA,
            VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_SUBTRACT,
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
        }
    };

    vsg::GraphicsPipelineStates pipelineStates{
        vsg::VertexInputState::create(vertexBindingsDescriptions, vertexAttributeDescriptions),
        vsg::InputAssemblyState::create(),
        rasterState,
        vsg::MultisampleState::create(),
        colorBlendState,
        vsg::DepthStencilState::create()
    };

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, pushConstantRanges);
    auto pipeline = vsg::GraphicsPipeline::create(pipelineLayout, vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
    bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(pipeline);

    // creating framebuffers and attachments
    vsg::AttachmentDescription albedo = vsg::defaultColorAttachment(VK_FORMAT_R8G8B8A8_UNORM);
    albedo.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    vsg::AttachmentDescription position = vsg::defaultColorAttachment(VK_FORMAT_R32G32B32_SFLOAT);
    position.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    vsg::AttachmentDescription normal = vsg::defaultColorAttachment(VK_FORMAT_R32G32_SFLOAT);
    normal.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    vsg::AttachmentDescription material = vsg::defaultColorAttachment(VK_FORMAT_R32G32B32A32_SFLOAT);
    material.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    vsg::AttachmentDescription depth = vsg::defaultDepthAttachment(VK_FORMAT_R32_SFLOAT);
    vsg::RenderPass::Attachments attachments{albedo, position, normal, material, depth};

    VkAttachmentReference albedoAttachmentRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference positionAttachmentRef{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference normalAttachmentRef{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference materialAttachmentRef{3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthlAttachmentRef{4, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL};

    vsg::SubpassDescription subpass{
        0, VK_PIPELINE_BIND_POINT_GRAPHICS, {},
        {albedoAttachmentRef, positionAttachmentRef, normalAttachmentRef, materialAttachmentRef}, {},
        {depthlAttachmentRef}, {}
    };
    vsg::RenderPass::Subpasses subpasses{subpass};

    VkSubpassDependency albedoDependency{
        VK_SUBPASS_EXTERNAL, 0,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0
    };
    VkSubpassDependency positionDependency{
        VK_SUBPASS_EXTERNAL, 0,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0
    };
    VkSubpassDependency normalDependency{
        VK_SUBPASS_EXTERNAL, 0,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0
    };
    VkSubpassDependency materiallDependency{
        VK_SUBPASS_EXTERNAL, 0,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0
    };
    VkSubpassDependency depthDependency{
        VK_SUBPASS_EXTERNAL, 0,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 0
    };

    vsg::RenderPass::Dependencies dependencies{
        albedoDependency, positionDependency, normalDependency, materiallDependency, depthDependency
    };

    auto renderPass = vsg::RenderPass::create(device, attachments, subpasses, dependencies);

    auto depthImage = vsg::Image::create();
    depthImage->format = VK_FORMAT_R32_SFLOAT;
    depthImage->extent = {width, height, 1};
    depthImage->mipLevels = 1;
    depthImage->arrayLayers = 1;
    depthImage->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    vsg::ImageViews attachmentViews{
        gBuffer->albedo->imageInfoList[0]->imageView,
        gBuffer->depth->imageInfoList[0]->imageView,
        gBuffer->normal->imageInfoList[0]->imageView,
        gBuffer->material->imageInfoList[0]->imageView,
        vsg::ImageView::create(depthImage)
    };

    frameBuffer = vsg::Framebuffer::create(renderPass, attachmentViews, width, height, 1);

    // we are using a rendergraph to set the framebuffer and renderpass
    // see https://github.com/vsg-dev/vsgExamples/blob/master/examples/viewer/vsgrendertotexture/vsgrendertotexture.cpp
    // for usage of offline rendering
    auto renderGraph = vsg::RenderGraph::create();
    renderGraph->renderArea.offset = {0, 0};
    renderGraph->renderArea.extent = {width, height};
    renderGraph->framebuffer = frameBuffer;

    renderGraph->clearValues.resize(dependencies.size(), {});
    renderGraph->clearValues.back().depthStencil = {1, 0};

    //TODO: maybe create viewer already here, however requires scenen information.
    // According to my current knowledge this is preferred, as the scene graph has to be transformed anyway to only use this graphics pipeline
}

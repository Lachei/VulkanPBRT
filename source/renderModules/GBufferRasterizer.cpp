#include <renderModules/GBufferRasterizer.hpp>

GBufferRasterizer::GBufferRasterizer(vsg::Device* device, uint32_t width, uint32_t height, bool doubleSided, bool blend)
    : g_buffer(GBuffer::create(width, height)),
      _width(width),
      _height(height),
      _double_sided(doubleSided),
      _blend(blend),
      _device(device)
{
    setup_graphics_pipeline();
}
void GBufferRasterizer::compile(vsg::Context& context)
{
    g_buffer->compile(context);
}
void GBufferRasterizer::update_image_layouts(vsg::Context& context)
{
    g_buffer->update_image_layouts(context);
}
void GBufferRasterizer::setup_graphics_pipeline()
{
    // loading shaders and getting the descriptor set layout from the shader
    std::string vertex_path = "shaders/rasterizer.vert.spv";
    std::string fragment_path = "shaders/rasterizer.frag.spv";

    auto vertex_shader = vsg::ShaderStage::read(VK_SHADER_STAGE_VERTEX_BIT, "main", vertex_path);
    auto fragment_shader = vsg::ShaderStage::read(VK_SHADER_STAGE_FRAGMENT_BIT, "main", fragment_path);
    vsg::BindingMap binding_map = vsg::ShaderStage::mergeBindingMaps(
        {vertex_shader->getDescriptorSetLayoutBindingsMap(), fragment_shader->getDescriptorSetLayoutBindingsMap()});

    auto descriptor_set_layout = vsg::DescriptorSetLayout::create(binding_map.begin()->second.bindings);

    // code mostly copied from vsg::src::vsgXchange::assimp::assimp.cpp
    vsg::PushConstantRanges push_constant_ranges{
        {VK_SHADER_STAGE_VERTEX_BIT, 0, 128}
  // projection view, and model matrices, actual push constant calls autoaatically provided by the VSG's
  // DispatchTraversal
    };

    vsg::VertexInputState::Bindings vertex_bindings_descriptions{
        VkVertexInputBindingDescription{0, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // vertex data
        VkVertexInputBindingDescription{1, sizeof(vsg::vec3), VK_VERTEX_INPUT_RATE_VERTEX}, // normal data
        VkVertexInputBindingDescription{2, sizeof(vsg::vec2), VK_VERTEX_INPUT_RATE_VERTEX}  // texcoord data
    };

    vsg::VertexInputState::Attributes vertex_attribute_descriptions{
        VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}, // vertex data
        VkVertexInputAttributeDescription{1, 1, VK_FORMAT_R32G32B32_SFLOAT, 0}, // normal data
        VkVertexInputAttributeDescription{2, 2, VK_FORMAT_R32G32_SFLOAT,    0}  // texcoord data
    };

    auto raster_state = vsg::RasterizationState::create();
    raster_state->cullMode = _double_sided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;

    auto color_blend_state = vsg::ColorBlendState::create();
    color_blend_state->attachments = vsg::ColorBlendState::ColorBlendAttachments{
        {_blend, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_ADD,
         VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_OP_SUBTRACT,
         VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT}
    };

    vsg::GraphicsPipelineStates pipeline_states{
        vsg::VertexInputState::create(vertex_bindings_descriptions, vertex_attribute_descriptions),
        vsg::InputAssemblyState::create(), raster_state, vsg::MultisampleState::create(), color_blend_state,
        vsg::DepthStencilState::create()};

    auto pipeline_layout
        = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptor_set_layout}, push_constant_ranges);
    auto pipeline = vsg::GraphicsPipeline::create(
        pipeline_layout, vsg::ShaderStages{vertex_shader, fragment_shader}, pipeline_states);
    bind_graphics_pipeline = vsg::BindGraphicsPipeline::create(pipeline);

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

    VkAttachmentReference albedo_attachment_ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference position_attachment_ref{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference normal_attachment_ref{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference material_attachment_ref{3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthl_attachment_ref{4, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL};

    vsg::SubpassDescription subpass{
        0, VK_PIPELINE_BIND_POINT_GRAPHICS, {                     },
        {albedo_attachment_ref,  position_attachment_ref, normal_attachment_ref, material_attachment_ref},
          {           },
        {depthl_attachment_ref                      },
          {                      }
    };
    vsg::RenderPass::Subpasses subpasses{subpass};

    VkSubpassDependency albedo_dependency{VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0};
    VkSubpassDependency position_dependency{VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0};
    VkSubpassDependency normal_dependency{VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0};
    VkSubpassDependency materiall_dependency{VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0};
    VkSubpassDependency depth_dependency{VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 0};

    vsg::RenderPass::Dependencies dependencies{
        albedo_dependency, position_dependency, normal_dependency, materiall_dependency, depth_dependency};

    auto render_pass = vsg::RenderPass::create(_device, attachments, subpasses, dependencies);

    auto depth_image = vsg::Image::create();
    depth_image->format = VK_FORMAT_R32_SFLOAT;
    depth_image->extent = {_width, _height, 1};
    depth_image->mipLevels = 1;
    depth_image->arrayLayers = 1;
    depth_image->usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    vsg::ImageViews attachment_views{g_buffer->albedo->imageInfoList[0]->imageView,
        g_buffer->depth->imageInfoList[0]->imageView, g_buffer->normal->imageInfoList[0]->imageView,
        g_buffer->material->imageInfoList[0]->imageView, vsg::ImageView::create(depth_image)};

    frame_buffer = vsg::Framebuffer::create(render_pass, attachment_views, _width, _height, 1);

    // we are using a rendergraph to set the framebuffer and renderpass
    // see https://github.com/vsg-dev/vsgExamples/blob/master/examples/viewer/vsgrendertotexture/vsgrendertotexture.cpp
    // for usage of offline rendering
    auto render_graph = vsg::RenderGraph::create();
    render_graph->renderArea.offset = {0, 0};
    render_graph->renderArea.extent = {_width, _height};
    render_graph->framebuffer = frame_buffer;

    render_graph->clearValues.resize(dependencies.size(), {});
    render_graph->clearValues.back().depthStencil = {1, 0};

    // TODO: maybe create viewer already here, however requires scenen information.
    //  According to my current knowledge this is preferred, as the scene graph has to be transformed anyway to only use
    //  this graphics pipeline
}

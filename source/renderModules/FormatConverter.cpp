#include <renderModules/FormatConverter.hpp>
#include <vsgXchange/glsl.h>

FormatConverter::FormatConverter(
    vsg::ref_ptr<vsg::ImageView> src_image, VkFormat dst_format, int work_width, int work_height)
    : _width(src_image->image->extent.width),
      _height(src_image->image->extent.height),
      _work_width(work_width),
      _work_height(work_height)
{
    std::vector<std::string> defines;
    switch (dst_format)
    {
    case VK_FORMAT_B8G8R8A8_UNORM:
        defines.emplace_back("FORMAT rgba8");
        break;
    default:
        throw vsg::Exception{"FormatConverter::Unknown format"};
    }
    auto options = vsg::Options::create(vsgXchange::glsl::create());
    auto compute_stage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", _shader_path, options);
    compute_stage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::intValue::create(work_width) },
        {1, vsg::intValue::create(work_height)}
    };
    auto compile_hints = vsg::ShaderCompileSettings::create();
    compile_hints->vulkanVersion = VK_API_VERSION_1_2;
    compile_hints->target = vsg::ShaderCompileSettings::SPIRV_1_4;
    compile_hints->defines = defines;
    compute_stage->module->hints = compile_hints;

    auto binding_map = compute_stage->getDescriptorSetLayoutBindingsMap();
    auto descriptor_set_layout = vsg::DescriptorSetLayout::create(binding_map.begin()->second.bindings);
    auto pipeline_layout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts{descriptor_set_layout}, compute_stage->getPushConstantRanges());
    auto pipeline = vsg::ComputePipeline::create(pipeline_layout, compute_stage);
    int final_index = vsg::ShaderStage::getSetBindingIndex(binding_map, "final").second;
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = dst_format;
    image->extent = src_image->image->extent;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    auto image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    auto image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    final_image = vsg::DescriptorImage::create(image_info, final_index, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    int source_index = vsg::ShaderStage::getSetBindingIndex(binding_map, "source").second;
    src_image->image->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info = vsg::ImageInfo::create(vsg::Sampler::create(), src_image, VK_IMAGE_LAYOUT_GENERAL);
    auto src_dsc_image
        = vsg::DescriptorImage::create(image_info, source_index, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    auto descriptor_set
        = vsg::DescriptorSet::create(pipeline_layout->setLayouts[0], vsg::Descriptors{final_image, src_dsc_image});
    _bind_descriptor_set
        = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, descriptor_set);

    _bind_pipeline = vsg::BindComputePipeline::create(pipeline);
}
void FormatConverter::compile_images(vsg::Context& context) const
{
    final_image->compile(context);
}
void FormatConverter::update_image_layouts(vsg::Context& context) const
{
    VkImageSubresourceRange resource_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto final_layout
        = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL, 0, 0, final_image->imageInfoList[0]->imageView->image, resource_range);
    auto pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, final_layout);
    context.commands.emplace_back(pipeline_barrier);
}
void FormatConverter::add_dispatch_to_command_graph(vsg::ref_ptr<vsg::Commands> command_graph)
{
    command_graph->addChild(_bind_pipeline);
    command_graph->addChild(_bind_descriptor_set);
    command_graph->addChild(
        vsg::Dispatch::create(uint32_t(ceil(static_cast<float>(_width) / static_cast<float>(_work_width))),
            uint32_t(ceil(static_cast<float>(_height) / static_cast<float>(_work_height))), 1));
}

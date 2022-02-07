#include <renderModules/denoisers/BFRBlender.hpp>

#include <string>



BFRBlender::BFRBlender(uint32_t width, uint32_t height, vsg::ref_ptr<vsg::DescriptorImage> average_image,
    vsg::ref_ptr<vsg::DescriptorImage> average_squared_image, vsg::ref_ptr<vsg::DescriptorImage> denoised0,
    vsg::ref_ptr<vsg::DescriptorImage> denoised1, vsg::ref_ptr<vsg::DescriptorImage> denoised2,
    uint32_t work_width, uint32_t work_height,
    uint32_t filter_radius) :
    _width(width), _height(height), _work_width(work_width), _work_height(work_height), _filter_radius(filter_radius)
{
    std::string shader_path = "shaders/bfrBlender.comp.spv";
    auto compute_stage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", shader_path);
    compute_stage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::intValue::create(width)},
        {1, vsg::intValue::create(height)},
        {2, vsg::intValue::create(work_height)},
        {3, vsg::intValue::create(work_width)},
        {4, vsg::intValue::create(filter_radius)}
    };

    //final image
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_B8G8R8A8_UNORM;
    image->extent.width = width;
    image->extent.height = height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    auto image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    auto image_info = vsg::ImageInfo::create( vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL );
    _final_image = vsg::DescriptorImage::create(image_info, _final_binding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    vsg::DescriptorSetLayoutBindings descriptor_bindings{
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    };
    auto descriptor_set_layout = vsg::DescriptorSetLayout::create(descriptor_bindings);
    auto descriptor_set = vsg::DescriptorSet::create(descriptor_set_layout, vsg::Descriptors{
                                                        vsg::DescriptorImage::create(
                                                            average_image->imageInfoList[0], _average_binding, 0,
                                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
                                                        vsg::DescriptorImage::create(
                                                            average_squared_image->imageInfoList[0], _average_squared_binding, 0,
                                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
                                                        vsg::DescriptorImage::create(
                                                            denoised0->imageInfoList[0], _denoised0_binding, 0,
                                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
                                                        vsg::DescriptorImage::create(
                                                            denoised1->imageInfoList[0], _denoised1_binding, 0,
                                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
                                                        vsg::DescriptorImage::create(
                                                            denoised2->imageInfoList[0], _denoised2_binding, 0,
                                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
                                                        _final_image
        });

    auto pipeline_layout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{ descriptor_set_layout }, vsg::PushConstantRanges{});
    _bind_descriptor_set = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, descriptor_set);

    _pipeline = vsg::ComputePipeline::create(pipeline_layout, compute_stage);
    _bind_pipeline = vsg::BindComputePipeline::create(_pipeline);
}
void BFRBlender::compile(vsg::Context& context)
{
    _final_image->compile(context);
}
void BFRBlender::update_image_layouts(vsg::Context& context)
{
    VkImageSubresourceRange resource_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto final_layout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                       VK_IMAGE_LAYOUT_GENERAL, 0, 0, _final_image->imageInfoList[0]->imageView->image,
                                                       resource_range);
    auto pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                        VK_DEPENDENCY_BY_REGION_BIT,
                                                        final_layout);
    context.commands.push_back(pipeline_barrier);
}
void BFRBlender::add_dispatch_to_command_graph(vsg::ref_ptr<vsg::Commands> command_graph)
{
    command_graph->addChild(_bind_pipeline);
    command_graph->addChild(_bind_descriptor_set);
    command_graph->addChild(vsg::Dispatch::create(uint32_t(ceil(static_cast<float>(_width) / static_cast<float>(_work_width))),
        uint32_t(ceil(static_cast<float>(_height) / static_cast<float>(_work_height))), 1));
}
void BFRBlender::copy_final_image(vsg::ref_ptr<vsg::Commands> commands, vsg::ref_ptr<vsg::Image> dst_image)
{
    auto src_image = _final_image->imageInfoList[0]->imageView->image;

    VkImageSubresourceRange resource_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto src_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0, src_image, resource_range);
    auto dst_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, dst_image, resource_range);
    auto pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                        VK_DEPENDENCY_BY_REGION_BIT,
                                                        src_barrier, dst_barrier);
    commands->addChild(pipeline_barrier);

    VkImageCopy copy_region{};
    copy_region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy_region.srcOffset = {0, 0, 0};
    copy_region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy_region.dstOffset = {0, 0, 0};
    copy_region.extent = {_width, _height, 1};

    auto copy_image = vsg::CopyImage::create();
    copy_image->srcImage = src_image;
    copy_image->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copy_image->dstImage = dst_image;
    copy_image->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copy_image->regions.emplace_back(copy_region);
    commands->addChild(copy_image);

    src_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                 VK_IMAGE_LAYOUT_GENERAL, 0, 0, src_image, resource_range);
    dst_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                 VK_IMAGE_LAYOUT_GENERAL, 0, 0, dst_image, resource_range);
    pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                   VK_DEPENDENCY_BY_REGION_BIT,
                                                   src_barrier, dst_barrier);
    commands->addChild(pipeline_barrier);
}
vsg::ref_ptr<vsg::DescriptorImage> BFRBlender::get_final_descriptor_image() const
{
    return _final_image;
}


#include <buffers/AccumulationBuffer.hpp>

AccumulationBuffer::AccumulationBuffer(uint32_t width, uint32_t height) : _width(width), _height(height)
{
    setup_images();
}
void AccumulationBuffer::update_descriptor(vsg::BindDescriptorSet* desc_set, const vsg::BindingMap& binding_map)
{
    int prev_illu_index = vsg::ShaderStage::getSetBindingIndex(binding_map, "prevOutput").second;
    prev_illu->dstBinding = prev_illu_index;
    int prev_illu_squared_index = vsg::ShaderStage::getSetBindingIndex(binding_map, "prevIlluminationSquared").second;
    prev_illu_squared->dstBinding = prev_illu_squared_index;
    int prev_depth_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "prevDepth").second;
    prev_depth->dstBinding = prev_depth_ind;
    int prev_normal_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "prevNormal").second;
    prev_normal->dstBinding = prev_normal_ind;
    int sample_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "sampleCounts").second;
    spp->dstBinding = sample_ind;
    int sample_acc_index = vsg::ShaderStage::getSetBindingIndex(binding_map, "prevSampleCounts").second;
    prev_spp->dstBinding = sample_acc_index;
    int motion_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "motion").second;
    motion->dstBinding = motion_ind;

    desc_set->descriptorSet->descriptors.push_back(prev_illu);
    desc_set->descriptorSet->descriptors.push_back(prev_illu_squared);
    desc_set->descriptorSet->descriptors.push_back(prev_depth);
    desc_set->descriptorSet->descriptors.push_back(prev_normal);
    desc_set->descriptorSet->descriptors.push_back(spp);
    desc_set->descriptorSet->descriptors.push_back(prev_spp);
    desc_set->descriptorSet->descriptors.push_back(motion);
}
void AccumulationBuffer::update_image_layouts(vsg::Context& context)
{
    VkImageSubresourceRange resource_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto prev_illu_layout
        = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL, 0, 0, prev_illu->imageInfoList[0]->imageView->image, resource_range);
    auto prev_illu_squared_layout
        = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL, 0, 0, prev_illu_squared->imageInfoList[0]->imageView->image, resource_range);
    auto prev_depth_layout
        = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL, 0, 0, prev_depth->imageInfoList[0]->imageView->image, resource_range);
    auto prev_normal_layout
        = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL, 0, 0, prev_normal->imageInfoList[0]->imageView->image, resource_range);
    auto spp_layout
        = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL, 0, 0, spp->imageInfoList[0]->imageView->image, resource_range);
    auto prev_spp_layout
        = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL, 0, 0, prev_spp->imageInfoList[0]->imageView->image, resource_range);
    auto motion_layout
        = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL, 0, 0, motion->imageInfoList[0]->imageView->image, resource_range);

    auto pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, prev_illu_layout,
        prev_illu_squared_layout, prev_depth_layout, prev_normal_layout, spp_layout, prev_spp_layout, motion_layout);
    context.commands.push_back(pipeline_barrier);
}
void AccumulationBuffer::compile(vsg::Context& context)
{
    prev_illu->compile(context);
    prev_illu_squared->compile(context);
    prev_depth->compile(context);
    prev_normal->compile(context);
    spp->compile(context);
    prev_spp->compile(context);
    motion->compile(context);
}
void AccumulationBuffer::copy_to_back_images(vsg::ref_ptr<vsg::Commands> commands, vsg::ref_ptr<GBuffer> g_buffer,
    vsg::ref_ptr<IlluminationBuffer> illumination_buffer)
{
    // depth image
    auto src_image = g_buffer->depth->imageInfoList[0]->imageView->image;
    auto dst_image = prev_depth->imageInfoList[0]->imageView->image;

    VkImageSubresourceRange resource_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto src_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0, src_image, resource_range);
    auto dst_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, dst_image, resource_range);
    auto pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, src_barrier, dst_barrier);
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

    src_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, src_image, resource_range);
    dst_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, dst_image, resource_range);
    pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, src_barrier, dst_barrier);
    commands->addChild(pipeline_barrier);

    // normal image
    src_image = g_buffer->normal->imageInfoList[0]->imageView->image;
    dst_image = prev_normal->imageInfoList[0]->imageView->image;

    src_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0, src_image, resource_range);
    dst_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, dst_image, resource_range);
    pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, src_barrier, dst_barrier);
    commands->addChild(pipeline_barrier);

    copy_image = vsg::CopyImage::create();
    copy_image->srcImage = src_image;
    copy_image->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copy_image->dstImage = dst_image;
    copy_image->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copy_image->regions.emplace_back(copy_region);
    commands->addChild(copy_image);

    src_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, src_image, resource_range);
    dst_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, dst_image, resource_range);
    pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, src_barrier, dst_barrier);
    commands->addChild(pipeline_barrier);

    // spp image
    src_image = spp->imageInfoList[0]->imageView->image;
    dst_image = prev_spp->imageInfoList[0]->imageView->image;

    src_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0, src_image, resource_range);
    dst_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, dst_image, resource_range);
    pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, src_barrier, dst_barrier);
    commands->addChild(pipeline_barrier);

    copy_image = vsg::CopyImage::create();
    copy_image->srcImage = src_image;
    copy_image->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copy_image->dstImage = dst_image;
    copy_image->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copy_image->regions.emplace_back(copy_region);
    commands->addChild(copy_image);

    src_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, src_image, resource_range);
    dst_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, dst_image, resource_range);
    pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, src_barrier, dst_barrier);
    commands->addChild(pipeline_barrier);

    // illumination
    if (illumination_buffer.cast<IlluminationBufferFinalDemodulated>())
    {
        src_image = illumination_buffer->illumination_images[1]->imageInfoList[0]->imageView->image;
    }
    else if (illumination_buffer.cast<IlluminationBufferDemodulated>())
    {
        src_image = illumination_buffer->illumination_images[0]->imageInfoList[0]->imageView->image;
    }
    else
    {
        throw vsg::Exception{"Error: AccumulationBuffer::copyToBackImages(...) Illumination buffer not supported."};
    }
    dst_image = prev_illu->imageInfoList[0]->imageView->image;

    src_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0, src_image, resource_range);
    dst_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, dst_image, resource_range);
    pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, src_barrier, dst_barrier);
    commands->addChild(pipeline_barrier);

    copy_image = vsg::CopyImage::create();
    copy_image->srcImage = src_image;
    copy_image->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copy_image->dstImage = dst_image;
    copy_image->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copy_image->regions.emplace_back(copy_region);
    commands->addChild(copy_image);

    src_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, src_image, resource_range);
    dst_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, dst_image, resource_range);
    pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, src_barrier, dst_barrier);
    commands->addChild(pipeline_barrier);

    // illumination squared
    if (illumination_buffer.cast<IlluminationBufferFinalDemodulated>())
    {
        src_image = illumination_buffer->illumination_images[2]->imageInfoList[0]->imageView->image;
    }
    else if (illumination_buffer.cast<IlluminationBufferDemodulated>())
    {
        src_image = illumination_buffer->illumination_images[1]->imageInfoList[0]->imageView->image;
    }
    else
    {
        throw vsg::Exception{"Error: AccumulationBuffer::copyToBackImages(...) Illumination buffer not supported."};
    }
    dst_image = prev_illu_squared->imageInfoList[0]->imageView->image;

    src_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0, src_image, resource_range);
    dst_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, dst_image, resource_range);
    pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, src_barrier, dst_barrier);
    commands->addChild(pipeline_barrier);

    copy_image = vsg::CopyImage::create();
    copy_image->srcImage = src_image;
    copy_image->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copy_image->dstImage = dst_image;
    copy_image->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copy_image->regions.emplace_back(copy_region);
    commands->addChild(copy_image);

    src_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, src_image, resource_range);
    dst_barrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, dst_image, resource_range);
    pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, src_barrier, dst_barrier);
    commands->addChild(pipeline_barrier);
}
void AccumulationBuffer::setup_images()
{
    auto sampler = vsg::Sampler::create();
    // creating sample accumulation image
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R8_UNORM;
    image->extent.width = _width;
    image->extent.height = _height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    auto image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    auto image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    spp = vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R8_UNORM;
    image->extent.width = _width;
    image->extent.height = _height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_info = vsg::ImageInfo::create(sampler, image_view, VK_IMAGE_LAYOUT_GENERAL);
    prev_spp = vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R32_SFLOAT;
    image->extent.width = _width;
    image->extent.height = _height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_info = vsg::ImageInfo::create(sampler, image_view, VK_IMAGE_LAYOUT_GENERAL);
    prev_depth = vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R32G32_SFLOAT;
    image->extent.width = _width;
    image->extent.height = _height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_info = vsg::ImageInfo::create(sampler, image_view, VK_IMAGE_LAYOUT_GENERAL);
    prev_normal = vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R16G16_SFLOAT;
    image->extent.width = _width;
    image->extent.height = _height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_STORAGE_BIT;
    image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    motion = vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R16G16B16A16_SFLOAT;
    image->extent.width = _width;
    image->extent.height = _height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_info = vsg::ImageInfo::create(sampler, image_view, VK_IMAGE_LAYOUT_GENERAL);
    prev_illu = vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R16G16B16A16_SFLOAT;
    image->extent.width = _width;
    image->extent.height = _height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_info = vsg::ImageInfo::create(sampler, image_view, VK_IMAGE_LAYOUT_GENERAL);
    prev_illu_squared = vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}

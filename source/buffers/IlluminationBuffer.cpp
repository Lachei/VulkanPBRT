#include <buffers/IlluminationBuffer.hpp>

#include <cassert>

void IlluminationBuffer::compile(vsg::Context& context)
{
    for (auto& image : illumination_images)
        image->compile(context);
}
void IlluminationBuffer::update_descriptor(vsg::BindDescriptorSet* desc_set, const vsg::BindingMap& binding_map)
{
    for (int i = 0; i < illumination_bindings.size(); ++i)
    {
        int index = vsg::ShaderStage::getSetBindingIndex(binding_map, illumination_bindings[i]).second;
        illumination_images[i]->dstBinding = index;
        desc_set->descriptorSet->descriptors.push_back(illumination_images[i]);
    }
}
void IlluminationBuffer::update_image_layouts(vsg::Context& context)
{
    VkImageSubresourceRange resource_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT);
    for (auto& image : illumination_images)
    {
        auto image_barrier
            = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_GENERAL, 0, 0, image->imageInfoList[0]->imageView->image, resource_range);
        pipeline_barrier->imageMemoryBarriers.push_back(image_barrier);
    }
    context.commands.push_back(pipeline_barrier);
}
void IlluminationBuffer::copy_image(
    vsg::ref_ptr<vsg::Commands> commands, uint32_t image_index, vsg::ref_ptr<vsg::Image> dst_image)
{
    assert(imageIndex < illuminationImages.size());
    auto src_image = illumination_images[image_index]->imageInfoList[0]->imageView->image;

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
    copy_region.extent = {width, height, 1};

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
}
IlluminationBufferFinal::IlluminationBufferFinal(uint32_t width, uint32_t height)
{
    this->width = width;
    this->height = height;
    illumination_bindings.push_back("outputImage");
    fill_images();
}
void IlluminationBufferFinal::fill_images()
{
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
    auto image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    illumination_images.push_back(vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));
}
IlluminationBufferFinalDirIndir::IlluminationBufferFinalDirIndir(uint32_t width, uint32_t height)
{
    this->width = width;
    this->height = height;
    illumination_bindings.push_back("outputImage");  // TODO change to appropriate value
    illumination_bindings.push_back("outputImage");
    illumination_bindings.push_back("outputImage");
    fill_images();
}
void IlluminationBufferFinalDirIndir::fill_images()
{
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
    auto image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    illumination_images.push_back(vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R32G32B32A32_SFLOAT;
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
    image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    illumination_images.push_back(vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R32G32B32A32_SFLOAT;
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
    image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    illumination_images.push_back(vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));
}
IlluminationBufferFinalDemodulated::IlluminationBufferFinalDemodulated(uint32_t width, uint32_t height)
{
    this->width = width;
    this->height = height;
    illumination_bindings.push_back("outputImage");
    illumination_bindings.push_back("illumination");
    illumination_bindings.push_back("illuminationSquared");
    fill_images();
}
void IlluminationBufferFinalDemodulated::fill_images()
{
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
    auto image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    illumination_images.push_back(vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R16G16B16A16_SFLOAT;
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
    image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    illumination_images.push_back(vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R16G16B16A16_SFLOAT;
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
    image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    illumination_images.push_back(vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));
}
IlluminationBufferDemodulated::IlluminationBufferDemodulated(uint32_t width, uint32_t height)
{
    this->width = width;
    this->height = height;
    illumination_bindings.push_back("illumination");
    illumination_bindings.push_back("illuminationSquared");
    fill_images();
}
void IlluminationBufferDemodulated::fill_images()
{
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R16G16B16A16_SFLOAT;
    image->extent.width = width;
    image->extent.height = height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    auto image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    auto image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    illumination_images.push_back(vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R16G16B16A16_SFLOAT;
    image->extent.width = width;
    image->extent.height = height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    illumination_images.push_back(vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));
}

IlluminationBufferDemodulatedFloat::IlluminationBufferDemodulatedFloat(uint32_t width, uint32_t height)
{
    this->width = width;
    this->height = height;
    illumination_bindings.push_back("illumination");
    illumination_bindings.push_back("illuminationSquared");
    fill_images();
}

void IlluminationBufferDemodulatedFloat::fill_images()
{
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R32G32B32A32_SFLOAT;
    image->extent.width = width;
    image->extent.height = height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    auto image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    auto image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    illumination_images.push_back(vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R32G32B32A32_SFLOAT;
    image->extent.width = width;
    image->extent.height = height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    illumination_images.push_back(vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));
}

IlluminatonBufferFinalFloat::IlluminatonBufferFinalFloat(uint32_t width, uint32_t height)
{
    this->width = width;
    this->height = height;
    illumination_bindings.push_back("outputImage");
    fill_images();
}

void IlluminatonBufferFinalFloat::fill_images()
{
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R32G32B32A32_SFLOAT;
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
    auto image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    illumination_images.push_back(vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));
}
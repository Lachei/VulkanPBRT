#include <buffers/AccumulationBuffer.hpp>

AccumulationBuffer::AccumulationBuffer(uint32_t width, uint32_t height) : width(width), height(height)
{
    setupImages();
}
void AccumulationBuffer::updateDescriptor(vsg::BindDescriptorSet* descSet, const vsg::BindingMap& bindingMap)
{
    int prevIlluIndex = vsg::ShaderStage::getSetBindingIndex(bindingMap, "prevOutput").second;
    prevIllu->dstBinding = prevIlluIndex;
    int prevIlluSquaredIndex = vsg::ShaderStage::getSetBindingIndex(bindingMap, "prevIlluminationSquared").second;
    prevIlluSquared->dstBinding = prevIlluSquaredIndex;
    int prevDepthInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "prevDepth").second;
    prevDepth->dstBinding = prevDepthInd;
    int prevNormalInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "prevNormal").second;
    prevNormal->dstBinding = prevNormalInd;
    int sampleInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "sampleCounts").second;
    spp->dstBinding = sampleInd;
    int sampleAccIndex = vsg::ShaderStage::getSetBindingIndex(bindingMap, "prevSampleCounts").second;
    prevSpp->dstBinding = sampleAccIndex;
    int motionInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "motion").second;
    motion->dstBinding = motionInd;

    descSet->descriptorSet->descriptors.push_back(prevIllu);
    descSet->descriptorSet->descriptors.push_back(prevIlluSquared);
    descSet->descriptorSet->descriptors.push_back(prevDepth);
    descSet->descriptorSet->descriptors.push_back(prevNormal);
    descSet->descriptorSet->descriptors.push_back(spp);
    descSet->descriptorSet->descriptors.push_back(prevSpp);
    descSet->descriptorSet->descriptors.push_back(motion);
}
void AccumulationBuffer::updateImageLayouts(vsg::Context& context)
{
    VkImageSubresourceRange resourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto prevIlluLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                          VK_IMAGE_LAYOUT_GENERAL, 0, 0, prevIllu->imageInfoList[0]->imageView->image,
                                                          resourceRange);
    auto prevIlluSquaredLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                                 VK_IMAGE_LAYOUT_GENERAL, 0, 0,
                                                                 prevIlluSquared->imageInfoList[0]->imageView->image, resourceRange);
    auto prevDepthLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                           VK_IMAGE_LAYOUT_GENERAL, 0, 0, prevDepth->imageInfoList[0]->imageView->image,
                                                           resourceRange);
    auto prevNormalLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                            VK_IMAGE_LAYOUT_GENERAL, 0, 0, prevNormal->imageInfoList[0]->imageView->image,
                                                            resourceRange);
    auto sppLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                     VK_IMAGE_LAYOUT_GENERAL, 0, 0, spp->imageInfoList[0]->imageView->image, resourceRange);
    auto prevSppLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                         VK_IMAGE_LAYOUT_GENERAL, 0, 0, prevSpp->imageInfoList[0]->imageView->image,
                                                         resourceRange);
    auto motionLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                        VK_IMAGE_LAYOUT_GENERAL, 0, 0, motion->imageInfoList[0]->imageView->image,
                                                        resourceRange);

    auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                                        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                                        prevIlluLayout, prevIlluSquaredLayout, prevDepthLayout, prevNormalLayout, sppLayout,
                                                        prevSppLayout, motionLayout);
    context.commands.push_back(pipelineBarrier);
}
void AccumulationBuffer::compile(vsg::Context& context)
{
    prevIllu->compile(context);
    prevIlluSquared->compile(context);
    prevDepth->compile(context);
    prevNormal->compile(context);
    spp->compile(context);
    prevSpp->compile(context);
    motion->compile(context);
}
void AccumulationBuffer::copyToBackImages(vsg::ref_ptr<vsg::Commands> commands, vsg::ref_ptr<GBuffer> gBuffer,
                                          vsg::ref_ptr<IlluminationBuffer> illuminationBuffer)
{
    // depth image
    auto srcImage = gBuffer->depth->imageInfoList[0]->imageView->image;
    auto dstImage = prevDepth->imageInfoList[0]->imageView->image;

    VkImageSubresourceRange resourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0, srcImage, resourceRange);
    auto dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, dstImage, resourceRange);
    auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                                        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                                        srcBarrier, dstBarrier);
    commands->addChild(pipelineBarrier);

    VkImageCopy copyRegion{};
    copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.srcOffset = {0, 0, 0};
    copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.dstOffset = {0, 0, 0};
    copyRegion.extent = {width, height, 1};

    auto copyImage = vsg::CopyImage::create();
    copyImage->srcImage = srcImage;
    copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copyImage->dstImage = dstImage;
    copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copyImage->regions.emplace_back(copyRegion);
    commands->addChild(copyImage);

    srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                 VK_IMAGE_LAYOUT_GENERAL, 0, 0, srcImage, resourceRange);
    dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                 VK_IMAGE_LAYOUT_GENERAL, 0, 0, dstImage, resourceRange);
    pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                                   VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                                   srcBarrier, dstBarrier);
    commands->addChild(pipelineBarrier);

    //normal image
    srcImage = gBuffer->normal->imageInfoList[0]->imageView->image;
    dstImage = prevNormal->imageInfoList[0]->imageView->image;

    srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0, srcImage, resourceRange);
    dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, dstImage, resourceRange);
    pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                                   VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                                   srcBarrier, dstBarrier);
    commands->addChild(pipelineBarrier);

    copyImage = vsg::CopyImage::create();
    copyImage->srcImage = srcImage;
    copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copyImage->dstImage = dstImage;
    copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copyImage->regions.emplace_back(copyRegion);
    commands->addChild(copyImage);

    srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                 VK_IMAGE_LAYOUT_GENERAL, 0, 0, srcImage, resourceRange);
    dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                 VK_IMAGE_LAYOUT_GENERAL, 0, 0, dstImage, resourceRange);
    pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                                   VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                                   srcBarrier, dstBarrier);
    commands->addChild(pipelineBarrier);

    // spp image
    srcImage = spp->imageInfoList[0]->imageView->image;
    dstImage = prevSpp->imageInfoList[0]->imageView->image;

    srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0, srcImage, resourceRange);
    dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, dstImage, resourceRange);
    pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                                   VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                                   srcBarrier, dstBarrier);
    commands->addChild(pipelineBarrier);

    copyImage = vsg::CopyImage::create();
    copyImage->srcImage = srcImage;
    copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copyImage->dstImage = dstImage;
    copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copyImage->regions.emplace_back(copyRegion);
    commands->addChild(copyImage);

    srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                 VK_IMAGE_LAYOUT_GENERAL, 0, 0, srcImage, resourceRange);
    dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                 VK_IMAGE_LAYOUT_GENERAL, 0, 0, dstImage, resourceRange);
    pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                                   VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                                   srcBarrier, dstBarrier);
    commands->addChild(pipelineBarrier);

    // illumination
    if (illuminationBuffer.cast<IlluminationBufferFinalDemodulated>())
        srcImage = illuminationBuffer->illuminationImages[1]->imageInfoList[0]->imageView->image;
    else if (illuminationBuffer.cast<IlluminationBufferDemodulated>())
        srcImage = illuminationBuffer->illuminationImages[0]->imageInfoList[0]->imageView->image;
    else
        throw vsg::Exception{"Error: AccumulationBuffer::copyToBackImages(...) Illumination buffer not supported."};
    dstImage = prevIllu->imageInfoList[0]->imageView->image;

    srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0, srcImage, resourceRange);
    dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, dstImage, resourceRange);
    pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                                   VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                                   srcBarrier, dstBarrier);
    commands->addChild(pipelineBarrier);

    copyImage = vsg::CopyImage::create();
    copyImage->srcImage = srcImage;
    copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copyImage->dstImage = dstImage;
    copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copyImage->regions.emplace_back(copyRegion);
    commands->addChild(copyImage);

    srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                 VK_IMAGE_LAYOUT_GENERAL, 0, 0, srcImage, resourceRange);
    dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                 VK_IMAGE_LAYOUT_GENERAL, 0, 0, dstImage, resourceRange);
    pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                                   VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                                   srcBarrier, dstBarrier);
    commands->addChild(pipelineBarrier);

    // illumination squared
    if (illuminationBuffer.cast<IlluminationBufferFinalDemodulated>())
        srcImage = illuminationBuffer->illuminationImages[2]->imageInfoList[0]->imageView->image;
    else if (illuminationBuffer.cast<IlluminationBufferDemodulated>())
        srcImage = illuminationBuffer->illuminationImages[1]->imageInfoList[0]->imageView->image;
    else
        throw vsg::Exception{"Error: AccumulationBuffer::copyToBackImages(...) Illumination buffer not supported."};
    dstImage = prevIlluSquared->imageInfoList[0]->imageView->image;

    srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0, srcImage, resourceRange);
    dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, dstImage, resourceRange);
    pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                                   VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                                   srcBarrier, dstBarrier);
    commands->addChild(pipelineBarrier);

    copyImage = vsg::CopyImage::create();
    copyImage->srcImage = srcImage;
    copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    copyImage->dstImage = dstImage;
    copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    copyImage->regions.emplace_back(copyRegion);
    commands->addChild(copyImage);

    srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                 VK_IMAGE_LAYOUT_GENERAL, 0, 0, srcImage, resourceRange);
    dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                 VK_IMAGE_LAYOUT_GENERAL, 0, 0, dstImage, resourceRange);
    pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                                   VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                                   srcBarrier, dstBarrier);
    commands->addChild(pipelineBarrier);
}
void AccumulationBuffer::setupImages()
{
    auto sampler = vsg::Sampler::create();
    //creating sample accumulation image
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R8_UNORM;
    image->extent.width = width;
    image->extent.height = height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    auto imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    auto imageInfo = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, imageView, VK_IMAGE_LAYOUT_GENERAL);
    spp = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R8_UNORM;
    image->extent.width = width;
    image->extent.height = height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    imageInfo = vsg::ImageInfo::create(sampler, imageView, VK_IMAGE_LAYOUT_GENERAL);
    prevSpp = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R32_SFLOAT;
    image->extent.width = width;
    image->extent.height = height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    imageInfo = vsg::ImageInfo::create(sampler, imageView, VK_IMAGE_LAYOUT_GENERAL);
    prevDepth = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R32G32_SFLOAT;
    image->extent.width = width;
    image->extent.height = height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    imageInfo = vsg::ImageInfo::create(sampler, imageView, VK_IMAGE_LAYOUT_GENERAL);
    prevNormal = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R16G16_SFLOAT;
    image->extent.width = width;
    image->extent.height = height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_STORAGE_BIT;
    imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    imageInfo = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, imageView, VK_IMAGE_LAYOUT_GENERAL);
    motion = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R16G16B16A16_SFLOAT;
    image->extent.width = width;
    image->extent.height = height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    imageInfo = vsg::ImageInfo::create(sampler, imageView, VK_IMAGE_LAYOUT_GENERAL);
    prevIllu = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R16G16B16A16_SFLOAT;
    image->extent.width = width;
    image->extent.height = height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    imageInfo = vsg::ImageInfo::create(sampler, imageView, VK_IMAGE_LAYOUT_GENERAL);
    prevIlluSquared = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}

#include <renderModules/denoisers/BFRBlender.hpp>

#include <string>



BFRBlender::BFRBlender(uint32_t width, uint32_t height, vsg::ref_ptr<vsg::DescriptorImage> averageImage,
    vsg::ref_ptr<vsg::DescriptorImage> averageSquaredImage, vsg::ref_ptr<vsg::DescriptorImage> denoised0,
    vsg::ref_ptr<vsg::DescriptorImage> denoised1, vsg::ref_ptr<vsg::DescriptorImage> denoised2,
    uint32_t workWidth, uint32_t workHeight,
    uint32_t filterRadius) :
    width(width), height(height), workWidth(workWidth), workHeight(workHeight), filterRadius(filterRadius)
{
    std::string shaderPath = "shaders/bfrBlender.comp.spv";
    auto computeStage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", shaderPath);
    computeStage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::intValue::create(width)},
        {1, vsg::intValue::create(height)},
        {2, vsg::intValue::create(workHeight)},
        {3, vsg::intValue::create(workWidth)},
        {4, vsg::intValue::create(filterRadius)}
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
    auto imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    auto imageInfo = vsg::ImageInfo::create( vsg::ref_ptr<vsg::Sampler>{}, imageView, VK_IMAGE_LAYOUT_GENERAL );
    finalImage = vsg::DescriptorImage::create(imageInfo, finalBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    vsg::DescriptorSetLayoutBindings descriptorBindings{
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
    };
    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{
                                                        vsg::DescriptorImage::create(
                                                            averageImage->imageInfoList[0], averageBinding, 0,
                                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
                                                        vsg::DescriptorImage::create(
                                                            averageSquaredImage->imageInfoList[0], averageSquaredBinding, 0,
                                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
                                                        vsg::DescriptorImage::create(
                                                            denoised0->imageInfoList[0], denoised0Binding, 0,
                                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
                                                        vsg::DescriptorImage::create(
                                                            denoised1->imageInfoList[0], denoised1Binding, 0,
                                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
                                                        vsg::DescriptorImage::create(
                                                            denoised2->imageInfoList[0], denoised2Binding, 0,
                                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
                                                        finalImage
        });

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{ descriptorSetLayout }, vsg::PushConstantRanges{});
    bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);

    pipeline = vsg::ComputePipeline::create(pipelineLayout, computeStage);
    bindPipeline = vsg::BindComputePipeline::create(pipeline);
}
void BFRBlender::compile(vsg::Context& context)
{
    finalImage->compile(context);
}
void BFRBlender::updateImageLayouts(vsg::Context& context)
{
    VkImageSubresourceRange resourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto finalLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                       VK_IMAGE_LAYOUT_GENERAL, 0, 0, finalImage->imageInfoList[0]->imageView->image,
                                                       resourceRange);
    auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                        VK_DEPENDENCY_BY_REGION_BIT,
                                                        finalLayout);
    context.commands.push_back(pipelineBarrier);
}
void BFRBlender::addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph)
{
    commandGraph->addChild(bindPipeline);
    commandGraph->addChild(bindDescriptorSet);
    commandGraph->addChild(vsg::Dispatch::create(uint32_t(ceil(float(width) / float(workWidth))),
        uint32_t(ceil(float(height) / float(workHeight))), 1));
}
void BFRBlender::copyFinalImage(vsg::ref_ptr<vsg::Commands> commands, vsg::ref_ptr<vsg::Image> dstImage)
{
    auto srcImage = finalImage->imageInfoList[0]->imageView->image;

    VkImageSubresourceRange resourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0, srcImage, resourceRange);
    auto dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL,
                                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, dstImage, resourceRange);
    auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                        VK_DEPENDENCY_BY_REGION_BIT,
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
    pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                   VK_DEPENDENCY_BY_REGION_BIT,
                                                   srcBarrier, dstBarrier);
    commands->addChild(pipelineBarrier);
}
vsg::ref_ptr<vsg::DescriptorImage> BFRBlender::getFinalDescriptorImage() const
{
    return finalImage;
}


#include <renderModules/Taa.hpp>
#include <renderModules/PipelineStructs.hpp>

Taa::Taa(uint32_t width, uint32_t height, uint32_t workWidth, uint32_t workHeight, vsg::ref_ptr<GBuffer> gBuffer,
    vsg::ref_ptr<AccumulationBuffer> accBuffer, vsg::ref_ptr<vsg::DescriptorImage> denoised) :
    width(width),
    height(height),
    workWidth(workWidth),
    workHeight(workHeight)
{
    denoised->imageInfoList[0]->imageView->image->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;

    sampler = vsg::Sampler::create();

    std::string shaderPath = "shaders/taa.comp.spv";
    auto computeStage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", shaderPath);
    computeStage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::intValue::create(width)},
        {1, vsg::intValue::create(height)},
        {2, vsg::intValue::create(workWidth)},
        {3, vsg::intValue::create(workHeight)}
    };

    //final image and accumulation creation
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R8G8B8A8_UNORM;
    image->extent.width = width;
    image->extent.height = height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    auto imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    auto imageInfo = vsg::ImageInfo::create( sampler, imageView, VK_IMAGE_LAYOUT_GENERAL );
    accumulationImage = vsg::DescriptorImage::create(imageInfo, accumulationBinding, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    image = vsg::Image::create();
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
    imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    imageInfo = vsg::ImageInfo::create( vsg::ref_ptr<vsg::Sampler>{}, imageView, VK_IMAGE_LAYOUT_GENERAL );
    finalImage = vsg::DescriptorImage::create(imageInfo, finalImageBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    //descriptor set layout
    auto bindingMap = computeStage->getDescriptorSetLayoutBindingsMap();
    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(bindingMap.begin()->second.bindings);
    auto denoisedInfo = denoised->imageInfoList[0];
    denoisedInfo->sampler = sampler;
    //filling descriptor set
    //vsg::Descriptors descriptors{vsg::DescriptorBuffer::create(vsg::BufferInfoList{vsg::BufferInfo(buffer, 0, bufferSize)}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)};
    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{
                                                        vsg::DescriptorImage::create(
                                                            accBuffer->motion->imageInfoList[0], motionBinding, 0,
                                                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
                                                        vsg::DescriptorImage::create(
                                                            denoisedInfo, denoisedBinding, 0,
                                                            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER),
                                                        finalImage,
                                                        accumulationImage
        });

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{ descriptorSetLayout },
        vsg::PushConstantRanges{
            {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(RayTracingPushConstants)}
        });
    bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);

    pipeline = vsg::ComputePipeline::create(pipelineLayout, computeStage);
    bindPipeline = vsg::BindComputePipeline::create(pipeline);
}
void Taa::compile(vsg::Context& context)
{
    finalImage->compile(context);
    accumulationImage->compile(context);
}
void Taa::updateImageLayouts(vsg::Context& context)
{
    VkImageSubresourceRange resourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    auto accumulationLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL, 0, 0,
        accumulationImage->imageInfoList[0]->imageView->image, resourceRange);
    auto finalLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL, 0, 0, finalImage->imageInfoList[0]->imageView->image,
        resourceRange);
    auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_DEPENDENCY_BY_REGION_BIT,
        accumulationLayout, finalLayout);
    context.commands.push_back(pipelineBarrier);
}
void Taa::addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph)
{
    commandGraph->addChild(bindPipeline);
    commandGraph->addChild(bindDescriptorSet);
    commandGraph->addChild(vsg::Dispatch::create(uint32_t(ceil(float(width) / float(workWidth))), uint32_t(ceil(float(height) / float(workHeight))),
                                                 1));
    copyFinalImage(commandGraph, accumulationImage->imageInfoList[0]->imageView->image);
}
void Taa::copyFinalImage(vsg::ref_ptr<vsg::Commands> commands, vsg::ref_ptr<vsg::Image> dstImage)
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
vsg::ref_ptr<vsg::DescriptorImage> Taa::getFinalDescriptorImage() const
{
    return finalImage;
}

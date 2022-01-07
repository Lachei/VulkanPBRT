#include <renderModules/denoisers/BMFR.hpp>

#include <renderModules/PipelineStructs.hpp>

BMFR::BMFR(uint32_t width, uint32_t height, uint32_t workWidth, uint32_t workHeight, vsg::ref_ptr<GBuffer> gBuffer,
    vsg::ref_ptr<IlluminationBuffer> illuBuffer, vsg::ref_ptr<AccumulationBuffer> accBuffer, uint32_t fittingKernel) :
    width(width),
    height(height),
    workWidth(workWidth),
    workHeight(workHeight),
    fittingKernel(fittingKernel),
    widthPadded((width / workWidth + 1) * workWidth),
    heightPadded((height / workHeight + 1) * workHeight),
    gBuffer(gBuffer),
    sampler(vsg::Sampler::create())
{
    if (!illuBuffer.cast<IlluminationBufferDemodulated>() && !illuBuffer.cast<IlluminationBufferDemodulatedFloat>()){
        std::cout << "Illumination Buffer type is required to be IlluminationBufferDemodulated/Float for BMFR" << std::endl;
        return; //demodulated illumination needed
    }  
    auto illumination = illuBuffer;
    //adding usage bits to illumination buffer
    illumination->illuminationImages[0]->imageInfoList[0]->imageView->image->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    std::string preShaderPath = "shaders/bmfrPre.comp.spv";
    std::string fitShaderPath = "shaders/bmfrFit.comp.spv";
    std::string postShaderPath = "shaders/bmfrPost.comp.spv";
    auto preComputeStage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", preShaderPath);
    auto fitComputeStage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", fitShaderPath);
    auto postComputeStage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", postShaderPath);
    preComputeStage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::intValue::create(width)},
        {1, vsg::intValue::create(height)},
        {2, vsg::intValue::create(workWidth)},
        {3, vsg::intValue::create(workHeight)},
        {4, vsg::intValue::create(workWidth)}
    };

    fitComputeStage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::intValue::create(width)},
        {1, vsg::intValue::create(height)},
        {2, vsg::intValue::create(fittingKernel)},
        {3, vsg::intValue::create(1)},
        {4, vsg::intValue::create(workWidth)}
    };

    postComputeStage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::intValue::create(width)},
        {1, vsg::intValue::create(height)},
        {2, vsg::intValue::create(workWidth)},
        {3, vsg::intValue::create(workHeight)},
        {4, vsg::intValue::create(workWidth)}
    };

    // denoised illuminatino accumulation
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R16G16B16A16_SFLOAT;
    image->extent.width = width;
    image->extent.height = height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 2;
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    auto imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    imageView->viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    auto imageInfo = vsg::ImageInfo::create( vsg::ref_ptr<vsg::Sampler>{}, imageView, VK_IMAGE_LAYOUT_GENERAL );
    accumulatedIllumination = vsg::DescriptorImage::create(imageInfo, denoisedBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    imageInfo->sampler = sampler;
    auto sampledAccIllu = vsg::DescriptorImage::create(imageInfo, sampledDenIlluBinding, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

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
    image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    imageInfo = vsg::ImageInfo::create( vsg::ref_ptr<vsg::Sampler>{}, imageView, VK_IMAGE_LAYOUT_GENERAL );
    finalIllumination = vsg::DescriptorImage::create(imageInfo, finalBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    //feature buffer image array
    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R16_SFLOAT;
    image->extent.width = widthPadded;
    image->extent.height = heightPadded;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = amtOfFeatures;
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    imageView->viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    imageInfo = vsg::ImageInfo::create( vsg::ref_ptr<vsg::Sampler>{}, imageView, VK_IMAGE_LAYOUT_GENERAL );
    featureBuffer = vsg::DescriptorImage::create(imageInfo, featureBufferBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    //weights buffer
    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_R32_SFLOAT;
    image->extent.width = widthPadded / workWidth;
    image->extent.height = heightPadded / workHeight;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = (amtOfFeatures - 3) * 3;
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    imageView->viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    imageInfo = vsg::ImageInfo::create( vsg::ref_ptr<vsg::Sampler>{}, imageView, VK_IMAGE_LAYOUT_GENERAL );
    weights = vsg::DescriptorImage::create(imageInfo, weightsBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    auto bindingMap = preComputeStage->getDescriptorSetLayoutBindingsMap();
    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(bindingMap.begin()->second.bindings);
    auto illuminationInfo = illumination->illuminationImages[0]->imageInfoList[0];
    illuminationInfo->sampler = sampler;
    // filling descriptor set
    vsg::Descriptors descriptors{
        vsg::DescriptorImage::create(gBuffer->depth->imageInfoList[0], depthBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
        vsg::DescriptorImage::create(gBuffer->normal->imageInfoList[0], normalBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
        vsg::DescriptorImage::create(gBuffer->material->imageInfoList[0], materialBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
        vsg::DescriptorImage::create(gBuffer->albedo->imageInfoList[0], albedoBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
        vsg::DescriptorImage::create(accBuffer->motion->imageInfoList[0], motionBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
        vsg::DescriptorImage::create(accBuffer->spp->imageInfoList[0], sampleBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
        vsg::DescriptorImage::create(illuminationInfo, noisyBinding, 0,
                                     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER),
        accumulatedIllumination,
        finalIllumination,
        sampledAccIllu,
        featureBuffer,
        weights
    };
    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, descriptors);

    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{ descriptorSetLayout },
        vsg::PushConstantRanges{
            {VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(RayTracingPushConstants)}
        });
    bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);

    bmfrPrePipeline = vsg::ComputePipeline::create(pipelineLayout, preComputeStage);
    bmfrFitPipeline = vsg::ComputePipeline::create(pipelineLayout, fitComputeStage);
    bmfrPostPipeline = vsg::ComputePipeline::create(pipelineLayout, postComputeStage);
    bindPrePipeline = vsg::BindComputePipeline::create(bmfrPrePipeline);
    bindFitPipeline = vsg::BindComputePipeline::create(bmfrFitPipeline);
    bindPostPipeline = vsg::BindComputePipeline::create(bmfrPostPipeline);
}
void BMFR::compile(vsg::Context& context)
{
    accumulatedIllumination->compile(context);
    finalIllumination->compile(context);
    featureBuffer->compile(context);
    weights->compile(context);
}
void BMFR::updateImageLayouts(vsg::Context& context)
{
    VkImageSubresourceRange resourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 2};
    auto accIlluLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                         VK_IMAGE_LAYOUT_GENERAL, 0, 0,
                                                         accumulatedIllumination->imageInfoList[0]->imageView->image, resourceRange);
    resourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto finalIluLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                          VK_IMAGE_LAYOUT_GENERAL, 0, 0,
                                                          finalIllumination->imageInfoList[0]->imageView->image, resourceRange);
    resourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, amtOfFeatures};
    auto featureBufferLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                               VK_IMAGE_LAYOUT_GENERAL, 0, 0,
                                                               featureBuffer->imageInfoList[0]->imageView->image, resourceRange);
    resourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, (amtOfFeatures - 3) * 3};
    auto weightsLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                         VK_IMAGE_LAYOUT_GENERAL, 0, 0, weights->imageInfoList[0]->imageView->image,
                                                         resourceRange);

    auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                        VK_DEPENDENCY_BY_REGION_BIT,
                                                        accIlluLayout, finalIluLayout, featureBufferLayout, weightsLayout);
    context.commands.push_back(pipelineBarrier);
}
void BMFR::addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph, vsg::ref_ptr<vsg::PushConstants> pushConstants)
{
    auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_DEPENDENCY_BY_REGION_BIT);
    uint32_t dispatchX = widthPadded / workWidth, dispatchY = heightPadded / workHeight;
    // pre pipeline
    commandGraph->addChild(bindPrePipeline);
    commandGraph->addChild(bindDescriptorSet);
    commandGraph->addChild(pushConstants);
    commandGraph->addChild(vsg::Dispatch::create(dispatchX, dispatchY, 1));
    commandGraph->addChild(pipelineBarrier);

    // fit pipeline
    commandGraph->addChild(bindFitPipeline);
    commandGraph->addChild(bindDescriptorSet);
    commandGraph->addChild(pushConstants);
    commandGraph->addChild(vsg::Dispatch::create(dispatchX, dispatchY, 1));
    commandGraph->addChild(pipelineBarrier);

    // post pipeline
    commandGraph->addChild(bindPostPipeline);
    commandGraph->addChild(bindDescriptorSet);
    commandGraph->addChild(pushConstants);
    commandGraph->addChild(vsg::Dispatch::create(dispatchX, dispatchY, 1));
    commandGraph->addChild(pipelineBarrier);
}
vsg::ref_ptr<vsg::DescriptorImage> BMFR::getFinalDescriptorImage() const
{
    return finalIllumination;
}

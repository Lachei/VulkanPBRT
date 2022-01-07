#include <renderModules/Accumulator.hpp>

Accumulator::Accumulator(vsg::ref_ptr<GBuffer> gBuffer, vsg::ref_ptr<IlluminationBuffer> illuminationBuffer, DoubleMatrices& matrices, int workWidth, int workHeight):
    width(gBuffer->depth->imageInfoList[0]->imageView->image->extent.width),
    height(gBuffer->depth->imageInfoList[0]->imageView->image->extent.height),
    matrices(matrices),
    workWidth(workWidth),
    workHeight(workHeight),
    accumulationBuffer(AccumulationBuffer::create(width, height)),
    accumulatedIllumination(IlluminationBufferDemodulated::create(width, height)),
    originalIllumination(illuminationBuffer)
{
    auto computeStage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", shaderPath);
    computeStage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::intValue::create(workWidth)}, 
        {1, vsg::intValue::create(workHeight)}
    };

    auto bindingMap = computeStage->getDescriptorSetLayoutBindingsMap();
    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(bindingMap.begin()->second.bindings);
    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, computeStage->getPushConstantRanges());
    auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeStage);
    bindPipeline = vsg::BindComputePipeline::create(pipeline);

    // adding image usage bit for illumination buffer type
    illuminationBuffer->illuminationImages[0]->imageInfoList[0]->imageView->image->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    vsg::Descriptors descriptors;
    auto imageInfo = vsg::ImageInfo::create(vsg::Sampler::create(), illuminationBuffer->illuminationImages[0]->imageInfoList[0]->imageView, VK_IMAGE_LAYOUT_GENERAL);
    int srcIndex = vsg::ShaderStage::getSetBindingIndex(bindingMap, "srcImage").second;
    auto descriptorImage = vsg::DescriptorImage::create(imageInfo, srcIndex);
    descriptors.push_back(descriptorImage);
    auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, descriptors);
    bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, descriptorSet);

    gBuffer->updateDescriptor(bindDescriptorSet, bindingMap);
    accumulationBuffer->updateDescriptor(bindDescriptorSet, bindingMap);
    accumulatedIllumination->updateDescriptor(bindDescriptorSet, bindingMap);
    
    pushConstantsValue = PCValue::create();
    pushConstantsValue->value().view = matrices[frameIndex].view;
    pushConstantsValue->value().invView = matrices[frameIndex].invView;
    pushConstantsValue->value().frameNumber = frameIndex;
    pushConstants = vsg::PushConstants::create(VK_SHADER_STAGE_COMPUTE_BIT, 0, pushConstantsValue);
}

void Accumulator::compileImages(vsg::Context &context) 
{
    accumulationBuffer->compile(context);
    accumulatedIllumination->compile(context);
}

void Accumulator::updateImageLayouts(vsg::Context& context) 
{
    accumulationBuffer->updateImageLayouts(context);
    accumulatedIllumination->updateImageLayouts(context);
}

void Accumulator::addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph) 
{
    commandGraph->addChild(bindPipeline);
    commandGraph->addChild(bindDescriptorSet);
    commandGraph->addChild(pushConstants);
    commandGraph->addChild(vsg::Dispatch::create(uint32_t(ceil(float(width) / float(workWidth))), uint32_t(ceil(float(height) / float(workHeight))),
                                                 1));
}

void Accumulator::setFrameIndex(int frame) 
{
    frameIndex = frame;
    pushConstantsValue->value().view = matrices[frameIndex].view;
    pushConstantsValue->value().invView = matrices[frameIndex].invView;
    if(frameIndex != 0){
        pushConstantsValue->value().prevView = matrices[frameIndex - 1].view;
        pushConstantsValue->value().prevPos = matrices[frameIndex - 1].invView[2];
        pushConstantsValue->value().prevPos /= pushConstantsValue->value().prevPos.w;
    }
    pushConstantsValue->value().frameNumber = frameIndex;
}

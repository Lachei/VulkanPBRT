#include "Accumulator.hpp"
Accumulator::Accumulator(vsg::ref_ptr<GBuffer> gBuffer, vsg::ref_ptr<IlluminationBuffer> illuminationBuffer, DoubleMatrices& matricesint, int workWidth, int workHeight):
    width(gBuffer->depth->imageInfoList[0].imageView->image->extent.width),
    height(gBuffer->depth->imageInfoList[0].imageView->image->extent.height),
    matrices(matrices),
    workWidth(workWidth),
    workHeight(workHeight),
    accumulationBuffer(AccumulationBuffer::create(width, height)),
    accumulatedIllumination(IlluminationBufferDemodulated::create(width, height))
{
    auto computeStage = vsg::ShaderStage::readSpv(VK_SHADER_STAGE_COMPUTE_BIT, "main", shaderPath);
    computeStage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::intValue::create(workWidth)}, 
        {1, vsg::intValue::create(workHeight)}
    };

    auto bindingMap = computeStage->getDescriptorSetLayoutBindingsMap();
    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(bindingMap.begin()->second.bindings);
    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, computeStage->getPushConstantRanges());
    auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeStage);

    // adding image usage bit for illumination buffer type
    illuminationBuffer->illuminationImages[0]->imageInfoList[0].imageView->image->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    vsg::Descriptors descriptors;
    vsg::ImageInfo imageInfo{vsg::Sampler::create(), illuminationBuffer->illuminationImages[0]->imageInfoList[0].imageView, VK_IMAGE_LAYOUT_GENERAL};
    int srcIndex = vsg::ShaderStage::getSetBindingIndex(bindingMap, "srcImage").second;
    auto descriptorImage = vsg::DescriptorImage::create(imageInfo, srcIndex);
    descriptors.push_back(descriptorImage);

    gBuffer->updateDescriptor(bindDescriptorSet, bindingMap);
    accumulationBuffer->updateDescriptor(bindDescriptorSet, bindingMap);
    
    pushConstants = PCValue::create();
    pushConstants->value().view = matrices[frameIndex].view;
    pushConstants->value().invView = matrices[frameIndex].invView;
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
}

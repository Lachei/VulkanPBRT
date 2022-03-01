#include <renderModules/Accumulator.hpp>
#include <vsgXchange/glsl.h>

Accumulator::Accumulator(vsg::ref_ptr<GBuffer> gBuffer, vsg::ref_ptr<IlluminationBuffer> illuminationBuffer, bool separateMatrices, int workWidth, int workHeight):
    width(gBuffer->depth->imageInfoList[0]->imageView->image->extent.width),
    height(gBuffer->depth->imageInfoList[0]->imageView->image->extent.height),
    workWidth(workWidth),
    workHeight(workHeight),
    accumulationBuffer(AccumulationBuffer::create(width, height)),
    accumulatedIllumination(IlluminationBufferDemodulated::create(width, height)),
    originalIllumination(illuminationBuffer),
    _separateMatrices(separateMatrices)
{
    auto options = vsg::Options::create(vsgXchange::glsl::create());
    auto computeStage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", shaderPath, options);
    if(!computeStage){
        throw vsg::Exception{"Accumulator::create() could not open compute shader stage"};
    }
    computeStage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::intValue::create(workWidth)}, 
        {1, vsg::intValue::create(workHeight)}
    };
    if(separateMatrices){
        auto compileHints = vsg::ShaderCompileSettings::create();
        compileHints->defines = {"SEPARATE_MATRICES"};
        computeStage->module->hints = compileHints;
    }

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
    auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0);
    commandGraph->addChild(bindPipeline);
    commandGraph->addChild(bindDescriptorSet);
    commandGraph->addChild(pushConstants);
    commandGraph->addChild(vsg::Dispatch::create(uint32_t(ceil(float(width) / float(workWidth))), uint32_t(ceil(float(height) / float(workHeight))),
                                                 1));
    commandGraph->addChild(pipelineBarrier);
}

void Accumulator::setCameraMatrices(int frameIndex, const CameraMatrices& cur, const CameraMatrices& prev)
{
    if(_separateMatrices){
        if(!cur.proj || !cur.invProj) throw vsg::Exception{"Accumulator::setDoubleMatrix(...) Accumulator was created with separateMatrices = true, but DubleMatrix is missing seperate matrices"};
        pushConstantsValue->value().view = cur.invProj.value();
        pushConstantsValue->value().invView = cur.invView;
        if(frameIndex){
            pushConstantsValue->value().prevView = prev.view;
            pushConstantsValue->value().prevPos = vsg::inverse(prev.view)[3];
            pushConstantsValue->value().prevPos.w = 1;
        }
        pushConstantsValue->value().frameNumber = frameIndex;
    }
    else{
        pushConstantsValue->value().view = cur.view;
        pushConstantsValue->value().invView = cur.invView;
        if(frameIndex){
            pushConstantsValue->value().prevView = prev.view;
            pushConstantsValue->value().prevPos = prev.invView[2];
            pushConstantsValue->value().prevPos /= pushConstantsValue->value().prevPos.w;
        }
        pushConstantsValue->value().frameNumber = frameIndex;
    }
}

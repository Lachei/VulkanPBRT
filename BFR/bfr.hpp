#pragma once
#include <vsg/all.h>
#include <string>
#include "../GBuffer.hpp"
#include "../IlluminationBuffer.hpp"
#include "../TAA/taa.hpp"

class BFR{
public:
    BFR(uint width, uint height, uint workWidth, uint workHeight):
    taaPipeline(width, height, workWidth, workHeight),
    width(width),
    height(height),
    workWidth(workWidth),
    workHeight(workHeight)
    {
        std::string shaderPath = "shader/bfr.comp.spv";
        auto computeStage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", shaderPath);
        computeStage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
            {0, vsg::intValue::create(width)},
            {1, vsg::intValue::create(height)},
            {2, vsg::intValue::create(workWidth)},
            {3, vsg::intValue::create(workHeight)}};

        //descriptor set layout
        vsg::DescriptorSetLayoutBindings descriptorBindings{{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        //filling descriptor set
        //vsg::Descriptors descriptors{vsg::DescriptorBuffer::create(vsg::BufferInfoList{vsg::BufferInfo(buffer, 0, bufferSize)}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)};
        auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{});

        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{});
        bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);

        bfrPipeline = vsg::ComputePipeline::create(pipelineLayout, computeStage);
        bindBfrPipeline = vsg::BindComputePipeline::create(bfrPipeline);
    }

    void addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph){
        commandGraph->addChild(bindBfrPipeline);
        commandGraph->addChild(bindDescriptorSet);
        commandGraph->addChild(vsg::Dispatch::create(uint(ceil(float(width) / float(workWidth))), uint(ceil(float(height) / float(workHeight))), 1));
        taaPipeline.addDispatchToCommandGraph(commandGraph);
    }

    vsg::ref_ptr<vsg::ComputePipeline> bfrPipeline;
    vsg::ref_ptr<vsg::BindComputePipeline> bindBfrPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;
    Taa taaPipeline;
protected:
    uint width, height, workWidth, workHeight;
};
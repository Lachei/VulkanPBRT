/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/io/Options.h>
#include <vsg/state/ComputePipeline.h>
#include <vsg/traversals/CompileTraversal.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

////////////////////////////////////////////////////////////////////////
//
// ComputePipeline
//
ComputePipeline::ComputePipeline()
{
}

ComputePipeline::ComputePipeline(PipelineLayout* pipelineLayout, ShaderStage* shaderStage) :
    layout(pipelineLayout),
    stage(shaderStage)
{
}

ComputePipeline::~ComputePipeline()
{
}

void ComputePipeline::read(Input& input)
{
    Object::read(input);

    input.readObject("PipelineLayout", layout);
    input.readObject("ShaderStage", stage);
}

void ComputePipeline::write(Output& output) const
{
    Object::write(output);

    output.writeObject("PipelineLayout", layout.get());
    output.writeObject("ShaderStage", stage.get());
}

void ComputePipeline::compile(Context& context)
{
    if (!_implementation[context.deviceID])
    {
        // compile shaders if required
        bool requiresShaderCompiler = stage && stage->module && stage->module->code.empty() && !(stage->module->source.empty());

        if (requiresShaderCompiler)
        {
            auto shaderCompiler = context.getOrCreateShaderCompiler();
            if (shaderCompiler)
            {
                shaderCompiler->compile(stage); // may need to map defines and paths in some fashion
            }
        }

        layout->compile(context);
        stage->compile(context);
        _implementation[context.deviceID] = ComputePipeline::Implementation::create(context, context.device, layout, stage);
    }
}

////////////////////////////////////////////////////////////////////////
//
// ComputePipeline::Implementation
//
ComputePipeline::Implementation::Implementation(Context& context, Device* device, PipelineLayout* pipelineLayout, ShaderStage* shaderStage) :
    _device(device)
{
    VkPipelineShaderStageCreateInfo stageInfo = {};
    stageInfo.pNext = nullptr;
    shaderStage->apply(context, stageInfo);

    VkComputePipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipelineLayout->vk(device->deviceID);
    pipelineInfo.stage = stageInfo;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.pNext = nullptr;

    if (VkResult result = vkCreateComputePipelines(*device, VK_NULL_HANDLE, 1, &pipelineInfo, _device->getAllocationCallbacks(), &_pipeline); result != VK_SUCCESS)
    {
        throw Exception{"Error: vsg::Pipeline::createCompute(...) failed to create VkPipeline.", result};
    }
}

ComputePipeline::Implementation::~Implementation()
{
    vkDestroyPipeline(*_device, _pipeline, _device->getAllocationCallbacks());
}

////////////////////////////////////////////////////////////////////////
//
// BindComputePipeline
//
BindComputePipeline::BindComputePipeline(ComputePipeline* in_pipeline) :
    Inherit(0), // slot 0
    pipeline(in_pipeline)
{
}

BindComputePipeline::~BindComputePipeline()
{
}

void BindComputePipeline::read(Input& input)
{
    StateCommand::read(input);

    input.readObject("ComputePipeline", pipeline);
}

void BindComputePipeline::write(Output& output) const
{
    StateCommand::write(output);

    output.writeObject("ComputePipeline", pipeline.get());
}

void BindComputePipeline::record(CommandBuffer& commandBuffer) const
{
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->vk(commandBuffer.deviceID));
    commandBuffer.setCurrentPipelineLayout(pipeline->layout);
}

void BindComputePipeline::compile(Context& context)
{
    if (pipeline) pipeline->compile(context);
}

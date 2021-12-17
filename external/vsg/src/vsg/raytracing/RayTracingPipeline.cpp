/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/raytracing/RayTracingPipeline.h>

#include <vsg/core/Exception.h>
#include <vsg/io/Options.h>
#include <vsg/traversals/CompileTraversal.h>
#include <vsg/vk/CommandBuffer.h>
#include <vsg/vk/Extensions.h>

using namespace vsg;

////////////////////////////////////////////////////////////////////////
//
// RayTracingPipeline
//
RayTracingPipeline::RayTracingPipeline()
{
}

RayTracingPipeline::RayTracingPipeline(PipelineLayout* pipelineLayout, const ShaderStages& shaderStages, const RayTracingShaderGroups& shaderGroups, ref_ptr<RayTracingShaderBindingTable> bindingTable, int maxRecursionDepth) :
    _pipelineLayout(pipelineLayout),
    _bindingTable(bindingTable),
    _shaderStages(shaderStages),
    _rayTracingShaderGroups(shaderGroups),
    _maxRecursionDepth(maxRecursionDepth)
{
}

RayTracingPipeline::~RayTracingPipeline()
{
}

void RayTracingPipeline::read(Input& input)
{
    Object::read(input);

    input.readObject("PipelineLayout", _pipelineLayout);

    _shaderStages.resize(input.readValue<uint32_t>("NumShaderStages"));
    for (auto& shaderStage : _shaderStages)
    {
        input.readObject("ShaderStage", shaderStage);
    }
}

void RayTracingPipeline::write(Output& output) const
{
    Object::write(output);

    output.writeObject("PipelineLayout", _pipelineLayout.get());

    output.writeValue<uint32_t>("NumShaderStages", _shaderStages.size());
    for (auto& shaderStage : _shaderStages)
    {
        output.writeObject("ShaderStage", shaderStage.get());
    }
}

void RayTracingPipeline::compile(Context& context)
{
    if (!_implementation[context.deviceID])
    {
        // compile shaders if required
        bool requiresShaderCompiler = false;
        for (auto& shaderStage : _shaderStages)
        {
            if (shaderStage->module)
            {
                if (shaderStage->module->code.empty() && !(shaderStage->module->source.empty()))
                {
                    requiresShaderCompiler = true;
                }
            }
        }

        if (requiresShaderCompiler)
        {
            auto shaderCompiler = context.getOrCreateShaderCompiler();
            if (shaderCompiler)
            {
                shaderCompiler->compile(_shaderStages); // may need to map defines and paths in some fashion
            }
        }

        _pipelineLayout->compile(context);

        for (auto& shaderStage : _shaderStages)
        {
            shaderStage->compile(context);
        }

        _implementation[context.deviceID] = RayTracingPipeline::Implementation::create(context, this);
    }
}

////////////////////////////////////////////////////////////////////////
//
// RayTracingPipeline::Implementation
//
RayTracingPipeline::Implementation::Implementation(Context& context, RayTracingPipeline* rayTracingPipeline) :
    _device(context.device),
    _pipelineLayout(rayTracingPipeline->getPipelineLayout()),
    _shaderStages(rayTracingPipeline->getShaderStages()),
    _shaderGroups(rayTracingPipeline->getRayTracingShaderGroups())
{

    auto pipelineLayout = rayTracingPipeline->getPipelineLayout();

    Extensions* extensions = Extensions::Get(_device, true);

    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.layout = pipelineLayout->vk(context.deviceID);
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.pNext = nullptr;

    auto shaderStages = rayTracingPipeline->getShaderStages();

    std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfo(shaderStages.size());
    for (size_t i = 0; i < shaderStages.size(); ++i)
    {
        const ShaderStage* shaderStage = shaderStages[i];
        shaderStageCreateInfo[i].pNext = nullptr;
        shaderStage->apply(context, shaderStageCreateInfo[i]);
    }

    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStageCreateInfo.size());
    pipelineInfo.pStages = shaderStageCreateInfo.data();

    // assign the RayTracingShaderGroups
    auto& rayTracingShaderGroups = rayTracingPipeline->getRayTracingShaderGroups();
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups(rayTracingShaderGroups.size());
    for (size_t i = 0; i < rayTracingShaderGroups.size(); ++i)
    {
        rayTracingShaderGroups[i]->applyTo(shaderGroups[i]);
    }

    pipelineInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
    pipelineInfo.pGroups = shaderGroups.data();

    pipelineInfo.maxPipelineRayRecursionDepth = rayTracingPipeline->maxRecursionDepth();

    VkResult result = extensions->vkCreateRayTracingPipelinesKHR(*_device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, _device->getAllocationCallbacks(), &_pipeline);
    if (result == VK_SUCCESS)
    {
        rayTracingPipeline->_bindingTable->pipeline = _pipeline;
        rayTracingPipeline->_bindingTable->shaderGroups = rayTracingShaderGroups;
        rayTracingPipeline->_bindingTable->compile(context);
    }
    else
    {
        throw Exception{"Error: vsg::Pipeline::createGraphics(...) failed to create VkPipeline.", result};
    }
}

RayTracingPipeline::Implementation::~Implementation()
{
    vkDestroyPipeline(*_device, _pipeline, _device->getAllocationCallbacks());
}

////////////////////////////////////////////////////////////////////////
//
// BindRayTracingPipeline
//
BindRayTracingPipeline::BindRayTracingPipeline(RayTracingPipeline* pipeline) :
    Inherit(0), // slot 0
    _pipeline(pipeline)
{
}

BindRayTracingPipeline::~BindRayTracingPipeline()
{
}

void BindRayTracingPipeline::read(Input& input)
{
    StateCommand::read(input);

    input.readObject("RayTracingPipeline", _pipeline);
}

void BindRayTracingPipeline::write(Output& output) const
{
    StateCommand::write(output);

    output.writeObject("RayTracingPipeline", _pipeline.get());
}

void BindRayTracingPipeline::record(CommandBuffer& commandBuffer) const
{
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, _pipeline->vk(commandBuffer.deviceID));
    commandBuffer.setCurrentPipelineLayout(_pipeline->getPipelineLayout());
}

void BindRayTracingPipeline::compile(Context& context)
{
    if (_pipeline) _pipeline->compile(context);
}

void BindRayTracingPipeline::release()
{
    if (_pipeline) _pipeline->release();
}

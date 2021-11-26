/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/io/Options.h>
#include <vsg/state/PipelineLayout.h>
#include <vsg/traversals/CompileTraversal.h>

using namespace vsg;

//////////////////////////////////////
//
// PipelineLayout
//
PipelineLayout::PipelineLayout() :
    flags(0)
{
}

PipelineLayout::PipelineLayout(const DescriptorSetLayouts& in_setLayouts, const PushConstantRanges& in_pushConstantRanges, VkPipelineLayoutCreateFlags in_flags) :
    flags(in_flags),
    setLayouts(in_setLayouts),
    pushConstantRanges(in_pushConstantRanges)
{
}

PipelineLayout::~PipelineLayout()
{
}

void PipelineLayout::read(Input& input)
{
    Object::read(input);

    input.readValue<uint32_t>("Flags", flags);

    setLayouts.resize(input.readValue<uint32_t>("NumDescriptorSetLayouts"));
    for (auto& descriptorLayout : setLayouts)
    {
        input.readObject("DescriptorSetLayout", descriptorLayout);
    }

    pushConstantRanges.resize(input.readValue<uint32_t>("NumPushConstantRanges"));
    for (auto& pushConstantRange : pushConstantRanges)
    {
        input.readValue<uint32_t>("stageFlags", pushConstantRange.stageFlags);
        input.read("offset", pushConstantRange.offset);
        input.read("size", pushConstantRange.size);
    }
}

void PipelineLayout::write(Output& output) const
{
    Object::write(output);

    output.writeValue<uint32_t>("Flags", flags);

    output.writeValue<uint32_t>("NumDescriptorSetLayouts", setLayouts.size());
    for (auto& descriptorLayout : setLayouts)
    {
        output.writeObject("DescriptorSetLayout", descriptorLayout);
    }

    output.writeValue<uint32_t>("NumPushConstantRanges", pushConstantRanges.size());
    for (auto& pushConstantRange : pushConstantRanges)
    {
        output.writeValue<uint32_t>("stageFlags", pushConstantRange.stageFlags);
        output.write("offset", pushConstantRange.offset);
        output.write("size", pushConstantRange.size);
    }
}

void PipelineLayout::compile(Context& context)
{
    if (!_implementation[context.deviceID])
    {
        for (auto dsl : setLayouts)
        {
            dsl->compile(context);
        }
        _implementation[context.deviceID] = PipelineLayout::Implementation::create(context.device, setLayouts, pushConstantRanges, flags);
    }
}

//////////////////////////////////////
//
// PipelineLayout::Implementation
//
PipelineLayout::Implementation::Implementation(Device* device, const DescriptorSetLayouts& descriptorSetLayouts, const PushConstantRanges& pushConstantRanges, VkPipelineLayoutCreateFlags flags) :
    _device(device)
{
    std::vector<VkDescriptorSetLayout> layouts;
    for (auto& dsl : descriptorSetLayouts)
    {
        layouts.push_back(dsl->vk(device->deviceID));
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo;
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.flags = flags;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
    pipelineLayoutInfo.pSetLayouts = layouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
    pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();
    pipelineLayoutInfo.pNext = nullptr;

    if (VkResult result = vkCreatePipelineLayout(*device, &pipelineLayoutInfo, _device->getAllocationCallbacks(), &_pipelineLayout); result != VK_SUCCESS)
    {
        throw Exception{"Error: Failed to create PipelineLayout.", result};
    }
}

PipelineLayout::Implementation::~Implementation()
{
    if (_pipelineLayout)
    {
        vkDestroyPipelineLayout(*_device, _pipelineLayout, _device->getAllocationCallbacks());
    }
}

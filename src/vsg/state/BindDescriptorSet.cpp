/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/Exception.h>
#include <vsg/core/compare.h>
#include <vsg/io/Options.h>
#include <vsg/state/BindDescriptorSet.h>
#include <vsg/traversals/CompileTraversal.h>
#include <vsg/viewer/View.h>
#include <vsg/vk/CommandBuffer.h>

using namespace vsg;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BindDescriptorSets
//
BindDescriptorSets::BindDescriptorSets() :
    Inherit(1), // slot 1
    pipelineBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS),
    firstSet(0)
{
}

int BindDescriptorSets::compare(const Object& rhs_object) const
{
    int result = StateCommand::compare(rhs_object);
    if (result != 0) return result;

    auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_value(pipelineBindPoint, rhs.pipelineBindPoint))) return result;
    if ((result = compare_pointer(layout, rhs.layout))) return result;
    if ((result = compare_value(firstSet, rhs.firstSet))) return result;
    return compare_pointer_container(descriptorSets, rhs.descriptorSets);
}

void BindDescriptorSets::read(Input& input)
{
    _vulkanData.clear();

    StateCommand::read(input);

    input.readObject("layout", layout);
    input.read("firstSet", firstSet);
    input.readObjects("descriptorSets", descriptorSets);
}

void BindDescriptorSets::write(Output& output) const
{
    StateCommand::write(output);

    output.writeObject("layout", layout);
    output.write("firstSet", firstSet);
    output.writeObjects("descriptorSets", descriptorSets);
}

void BindDescriptorSets::compile(Context& context)
{
    auto& vkd = _vulkanData[context.deviceID];

    // no need to compile if already compiled
    if (vkd._vkPipelineLayout != 0 && vkd._vkDescriptorSets.size() == descriptorSets.size()) return;

    layout->compile(context);
    vkd._vkPipelineLayout = layout->vk(context.deviceID);

    vkd._vkDescriptorSets.resize(descriptorSets.size());
    for (size_t i = 0; i < descriptorSets.size(); ++i)
    {
        descriptorSets[i]->compile(context);
        vkd._vkDescriptorSets[i] = descriptorSets[i]->vk(context.deviceID);
    }
}

void BindDescriptorSets::record(CommandBuffer& commandBuffer) const
{
    auto& vkd = _vulkanData[commandBuffer.deviceID];
    vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, vkd._vkPipelineLayout, firstSet, static_cast<uint32_t>(vkd._vkDescriptorSets.size()), vkd._vkDescriptorSets.data(), 0, nullptr);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// BindDescriptorSet
//
BindDescriptorSet::BindDescriptorSet() :
    pipelineBindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS),
    firstSet(0)
{
}

int BindDescriptorSet::compare(const Object& rhs_object) const
{
    int result = StateCommand::compare(rhs_object);
    if (result != 0) return result;

    auto& rhs = static_cast<decltype(*this)>(rhs_object);

    if ((result = compare_value(pipelineBindPoint, rhs.pipelineBindPoint))) return result;
    if ((result = compare_pointer(layout, rhs.layout))) return result;
    if ((result = compare_value(firstSet, rhs.firstSet))) return result;
    return compare_pointer(descriptorSet, rhs.descriptorSet);
}

void BindDescriptorSet::read(Input& input)
{
    _vulkanData.clear();

    StateCommand::read(input);

    input.readObject("layout", layout);
    input.read("firstSet", firstSet);
    input.readObject("descriptorSet", descriptorSet);
}

void BindDescriptorSet::write(Output& output) const
{
    StateCommand::write(output);

    output.writeObject("layout", layout);
    output.write("firstSet", firstSet);
    output.writeObject("descriptorSet", descriptorSet);
}

void BindDescriptorSet::compile(Context& context)
{
    auto& vkd = _vulkanData[context.deviceID];

    // no need to compile if already compiled
    if (vkd._vkPipelineLayout != 0 && vkd._vkDescriptorSet != 0) return;

    layout->compile(context);
    descriptorSet->compile(context);

    vkd._vkPipelineLayout = layout->vk(context.deviceID);
    vkd._vkDescriptorSet = descriptorSet->vk(context.deviceID);
}

void BindDescriptorSet::record(CommandBuffer& commandBuffer) const
{
    auto& vkd = _vulkanData[commandBuffer.deviceID];

    vkCmdBindDescriptorSets(commandBuffer, pipelineBindPoint, vkd._vkPipelineLayout, firstSet, 1, &(vkd._vkDescriptorSet), 0, nullptr);
}

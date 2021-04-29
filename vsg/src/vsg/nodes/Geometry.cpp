/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/BindIndexBuffer.h>
#include <vsg/io/ReaderWriter.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/traversals/RecordTraversal.h>

using namespace vsg;

/////////////////////////////////////////////////////////////////////////////////////////
//
//  Geometry node
//       vertex arrays
//       index arrays
//       draw + draw DrawIndexed
//
Geometry::Geometry(Allocator* allocator) :
    Inherit(allocator)
{
}

Geometry::~Geometry()
{
}

void Geometry::read(Input& input)
{
    Node::read(input);

    input.read("firstBinding", firstBinding);
    arrays.resize(input.readValue<uint32_t>("NumArrays"));
    for (auto& array : arrays)
    {
        input.readObject("Array", array);
    }

    input.readObject("Indices", indices);

    commands.resize(input.readValue<uint32_t>("NumCommands"));
    for (auto& command : commands)
    {
        input.readObject("Command", command);
    }
}

void Geometry::write(Output& output) const
{
    Node::write(output);

    output.write("firstBinding", firstBinding);
    output.writeValue<uint32_t>("NumArrays", arrays.size());
    for (auto& array : arrays)
    {
        output.writeObject("Array", array.get());
    }

    output.writeObject("Indices", indices.get());

    output.writeValue<uint32_t>("NumCommands", commands.size());
    for (auto& command : commands)
    {
        output.writeObject("Command", command.get());
    }
}

void Geometry::compile(Context& context)
{
    if (arrays.empty() || commands.empty())
    {
        // Geometry does not contain required arrays or commands
        return;
    }

    for (auto& command : commands)
    {
        command->compile(context);
    }

    auto& vkd = _vulkanData[context.deviceID];

    vkd = {};

    bool failure = false;

    if (indices)
    {
        DataList dataList;
        dataList.reserve(arrays.size() + 1);
        dataList.insert(dataList.end(), arrays.begin(), arrays.end());
        dataList.emplace_back(indices);

        auto bufferInfoList = vsg::createBufferAndTransferData(context, dataList, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
        if (!bufferInfoList.empty())
        {
            BufferInfoList vertexBufferInfo(bufferInfoList.begin(), bufferInfoList.begin() + arrays.size());

            for (auto& bufferInfo : vertexBufferInfo)
            {
                vkd.buffers.push_back(bufferInfo.buffer);
                vkd.vkBuffers.push_back(bufferInfo.buffer->vk(context.deviceID));
                vkd.offsets.push_back(bufferInfo.offset);
            }

            vkd.bufferInfo = bufferInfoList.back();
            vkd.indexType = computeIndexType(indices);
        }
        else
            failure = true;
    }
    else
    {
        auto vertexBufferInfo = vsg::createBufferAndTransferData(context, arrays, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
        if (!vertexBufferInfo.empty())
        {
            for (auto& bufferInfo : vertexBufferInfo)
            {
                vkd.buffers.push_back(bufferInfo.buffer);
                vkd.vkBuffers.push_back(bufferInfo.buffer->vk(context.deviceID));
                vkd.offsets.push_back(bufferInfo.offset);
            }
        }
        else
            failure = true;
    }

    if (failure)
    {
        //std::cout<<"Failed to create required arrays/indices buffers on GPU."<<std::endl;
        vkd = {};
        return;
    }
}

void Geometry::record(CommandBuffer& commandBuffer) const
{
    auto& vkd = _vulkanData[commandBuffer.deviceID];

    VkCommandBuffer cmdBuffer{commandBuffer};

    vkCmdBindVertexBuffers(cmdBuffer, firstBinding, static_cast<uint32_t>(vkd.vkBuffers.size()), vkd.vkBuffers.data(), vkd.offsets.data());

    if (indices)
    {
        vkCmdBindIndexBuffer(cmdBuffer, vkd.bufferInfo.buffer->vk(commandBuffer.deviceID), vkd.bufferInfo.offset, vkd.indexType);
    }

    for (auto& command : commands)
    {
        command->record(commandBuffer);
    }
}

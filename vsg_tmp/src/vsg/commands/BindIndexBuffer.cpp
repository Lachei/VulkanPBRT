/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/BindIndexBuffer.h>
#include <vsg/io/Options.h>
#include <vsg/vk/CommandBuffer.h>
#include <vsg/vk/Context.h>

using namespace vsg;

// provide definition as VK_INDEX_TYPE_UINT8_EXT is not available in all headers
#define VK_INDEX_TYPE_UINT8 static_cast<VkIndexType>(1000265000)

VkIndexType vsg::computeIndexType(const Data* indices)
{
    if (indices)
    {
        switch (indices->valueSize())
        {
        case (1): return VK_INDEX_TYPE_UINT8;
        case (2): return VK_INDEX_TYPE_UINT16;
        case (4): return VK_INDEX_TYPE_UINT32;
        default: break;
        }
    }
    // nothing valid assigned
    return VK_INDEX_TYPE_MAX_ENUM;
}

BindIndexBuffer::BindIndexBuffer(Data* indices) :
    _indices(indices)
{
}

BindIndexBuffer::~BindIndexBuffer()
{
    for (auto& vkd : _vulkanData)
    {
        if (vkd.bufferInfo.buffer)
        {
            vkd.bufferInfo.buffer->release(vkd.bufferInfo.offset, 0); // TODO, we don't locally have a size allocated
        }
    }
}

void BindIndexBuffer::read(Input& input)
{
    Command::read(input);

    // clear Vulkan objects
    _vulkanData.clear();

    // read the key indices data
    input.readObject("Indices", _indices);
}

void BindIndexBuffer::write(Output& output) const
{
    Command::write(output);

    // write indices data
    output.writeObject("Indices", _indices.get());
}

void BindIndexBuffer::compile(Context& context)
{
    // nothing to compile
    if (!_indices) return;

    auto& vkd = _vulkanData[context.deviceID];

    // check if already compiled
    if (vkd.bufferInfo.buffer) return;

    auto bufferInfoList = vsg::createBufferAndTransferData(context, {_indices}, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_SHARING_MODE_EXCLUSIVE);
    if (!bufferInfoList.empty())
    {
        vkd.bufferInfo = bufferInfoList.back();
        vkd.indexType = computeIndexType(_indices);
    }
}

void BindIndexBuffer::record(CommandBuffer& commandBuffer) const
{
    auto& vkd = _vulkanData[commandBuffer.deviceID];
    vkCmdBindIndexBuffer(commandBuffer, vkd.bufferInfo.buffer->vk(commandBuffer.deviceID), vkd.bufferInfo.offset, vkd.indexType);
}

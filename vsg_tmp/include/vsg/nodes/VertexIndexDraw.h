#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/Command.h>
#include <vsg/nodes/Node.h>
#include <vsg/state/BufferInfo.h>
#include <vsg/traversals/CompileTraversal.h>

namespace vsg
{

    class VSG_DECLSPEC VertexIndexDraw : public Inherit<Command, VertexIndexDraw>
    {
    public:
        VertexIndexDraw(Allocator* allocator = nullptr);

        void read(Input& input) override;
        void write(Output& output) const override;

        void compile(Context& context) override;
        void record(CommandBuffer& commandBuffer) const override;

        // vkCmdDrawIndexed settings
        // vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        uint32_t indexCount = 0;
        uint32_t instanceCount = 0;
        uint32_t firstIndex = 0;
        uint32_t vertexOffset = 0;
        uint32_t firstInstance = 0;

        uint32_t firstBinding = 0;
        DataList arrays;
        ref_ptr<Data> indices;

    protected:
        virtual ~VertexIndexDraw();

        struct VulkanData
        {
            std::vector<ref_ptr<Buffer>> buffers;
            std::vector<VkBuffer> vkBuffers;
            std::vector<VkDeviceSize> offsets;
            BufferInfo bufferInfo;
            VkIndexType indexType = VK_INDEX_TYPE_UINT16;
        };

        vk_buffer<VulkanData> _vulkanData;
    };
    VSG_type_name(vsg::VertexIndexDraw)

} // namespace vsg

#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/Command.h>
#include <vsg/state/BufferInfo.h>
#include <vsg/state/Descriptor.h>
#include <vsg/vk/vk_buffer.h>

namespace vsg
{

    /** Compute the VkIndexType from Data source's value size.*/
    extern VSG_DECLSPEC VkIndexType computeIndexType(const Data* indices);

    class VSG_DECLSPEC BindIndexBuffer : public Inherit<Command, BindIndexBuffer>
    {
    public:
        BindIndexBuffer() {}

        BindIndexBuffer(Data* indices);

        void setIndices(ref_ptr<Data> indices) { _indices = indices; }
        Data* getIndices() { return _indices; }
        const Data* getIndices() const { return _indices; }

        void read(Input& input) override;
        void write(Output& output) const override;

        void compile(Context& context) override;

        void record(CommandBuffer& commandBuffer) const override;

    protected:
        virtual ~BindIndexBuffer();

        ref_ptr<Data> _indices;

        struct VulkanData
        {
            BufferInfo bufferInfo;
            VkIndexType indexType;
        };

        vk_buffer<VulkanData> _vulkanData;
    };
    VSG_type_name(vsg::BindIndexBuffer);

} // namespace vsg

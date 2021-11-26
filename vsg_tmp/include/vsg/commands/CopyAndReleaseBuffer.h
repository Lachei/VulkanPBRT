#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <deque>
#include <memory>

#include <vsg/core/Object.h>
#include <vsg/core/ScratchMemory.h>
#include <vsg/nodes/Group.h>
#include <vsg/state/BufferInfo.h>
#include <vsg/state/GraphicsPipeline.h>
#include <vsg/vk/CommandPool.h>
#include <vsg/vk/DescriptorPool.h>
#include <vsg/vk/Fence.h>
#include <vsg/vk/MemoryBufferPools.h>

#include <vsg/commands/Command.h>

namespace vsg
{

    class VSG_DECLSPEC CopyAndReleaseBuffer : public Inherit<Command, CopyAndReleaseBuffer>
    {
    public:
        CopyAndReleaseBuffer(ref_ptr<MemoryBufferPools> optional_stagingMemoryBufferPools = {});

        void add(BufferInfo src, BufferInfo dest);

        /// MemoryBufferPools used for allocation staging buffer used by the copy(ref_ptr<Data>, BufferInfo) method.  Users should assign a MemoryBufferPools with appropriate settings.
        ref_ptr<MemoryBufferPools> stagingMemoryBufferPools;

        void copy(ref_ptr<Data> data, BufferInfo dest);

        void record(CommandBuffer& commandBuffer) const override;

    protected:
        virtual ~CopyAndReleaseBuffer();

        struct CopyData
        {
            BufferInfo source;
            BufferInfo destination;

            void record(CommandBuffer& commandBuffer) const;
        };

        mutable std::vector<CopyData> pending;
        mutable std::vector<CopyData> completed;
    };

} // namespace vsg

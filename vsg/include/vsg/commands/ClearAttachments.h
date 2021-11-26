#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2019 Thomas Hogarth

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/Command.h>

namespace vsg
{

    /// Encapsulation of vkCmdCopyAttachments functionality
    class VSG_DECLSPEC ClearAttachments : public Inherit<Command, ClearAttachments>
    {
    public:
        using Attachments = std::vector<VkClearAttachment>;
        using Rects = std::vector<VkClearRect>;

        ClearAttachments();
        ClearAttachments(const Attachments& in_attachments, const Rects& in_rects);

        Attachments attachments;
        Rects rects;

        void record(CommandBuffer& commandBuffer) const override;
    };
    VSG_type_name(vsg::ClearAttachments);

} // namespace vsg

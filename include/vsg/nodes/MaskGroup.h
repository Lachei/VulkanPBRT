#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/core/ref_ptr.h>

#include <vsg/nodes/Node.h>

#include <vector>

namespace vsg
{
    /// MaskGroup node for toggling on/off recording of children used a logical and operation between visitor.traversalMask and child.mask
    class VSG_DECLSPEC MaskGroup : public Inherit<Node, MaskGroup>
    {
    public:
        MaskGroup(Allocator* allocator = nullptr);

        struct Child
        {
            uint32_t mask = 0xffffff;
            ref_ptr<Node> node;
        };

        using Children = std::vector<Child>;

        template<class N, class V>
        static void t_traverse(N& node, V& visitor)
        {
            for (auto& child : node.children)
            {
                if ((visitor.traversalMask & (visitor.overrideMask | child.mask)) != 0) child.node->accept(visitor);
            }
        }

        void traverse(Visitor& visitor) override { t_traverse(*this, visitor); }
        void traverse(ConstVisitor& visitor) const override { t_traverse(*this, visitor); }
        void traverse(RecordTraversal& visitor) const override { t_traverse(*this, visitor); }

        void read(Input& input) override;
        void write(Output& output) const override;

        Children children;

        /// add a child to the back of the children list.
        void addChild(uint32_t mask, ref_ptr<Node> child);

    protected:
        virtual ~MaskGroup();
    };
    VSG_type_name(vsg::MaskGroup);

} // namespace vsg

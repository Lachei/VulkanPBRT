/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/Options.h>
#include <vsg/io/stream.h>
#include <vsg/nodes/CullNode.h>

using namespace vsg;

CullNode::CullNode(Allocator* allocator) :
    Inherit(allocator)
{
}

CullNode::CullNode(const dsphere& bound, Node* child, Allocator* allocator) :
    Inherit(allocator),
    _bound(bound),
    _child(child)
{
}

CullNode::~CullNode()
{
}

void CullNode::read(Input& input)
{
    Node::read(input);

    input.read("Bound", _bound);

    input.readObject("Child", _child);
}

void CullNode::write(Output& output) const
{
    Node::write(output);

    output.write("Bound", _bound);

    output.writeObject("Child", _child.get());
}

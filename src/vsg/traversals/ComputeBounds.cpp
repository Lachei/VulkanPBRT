/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/BindIndexBuffer.h>
#include <vsg/commands/BindVertexBuffers.h>
#include <vsg/commands/Commands.h>
#include <vsg/commands/Draw.h>
#include <vsg/commands/DrawIndexed.h>
#include <vsg/io/Options.h>
#include <vsg/nodes/Geometry.h>
#include <vsg/nodes/MatrixTransform.h>
#include <vsg/nodes/StateGroup.h>
#include <vsg/nodes/VertexIndexDraw.h>
#include <vsg/traversals/ComputeBounds.h>

using namespace vsg;

ComputeBounds::ComputeBounds(ref_ptr<ArrayState> intialArrayState)
{
    arrayStateStack.reserve(4);
    arrayStateStack.emplace_back(intialArrayState ? intialArrayState : ArrayState::create());
}

void ComputeBounds::apply(const vsg::Object& object)
{
    object.traverse(*this);
}

void ComputeBounds::apply(const StateGroup& stategroup)
{
    auto arrayState = stategroup.prototypeArrayState ? stategroup.prototypeArrayState->clone(arrayStateStack.back()) : arrayStateStack.back()->clone();

    for (auto& statecommand : stategroup.stateCommands)
    {
        statecommand->accept(*arrayState);
    }

    arrayStateStack.emplace_back(arrayState);

    stategroup.traverse(*this);

    arrayStateStack.pop_back();
}

void ComputeBounds::apply(const Transform& transform)
{
    if (matrixStack.empty())
        matrixStack.push_back(transform.transform(dmat4{}));
    else
        matrixStack.push_back(transform.transform(matrixStack.back()));

    transform.traverse(*this);

    matrixStack.pop_back();
}

void ComputeBounds::apply(const MatrixTransform& transform)
{
    if (matrixStack.empty())
        matrixStack.push_back(transform.matrix);
    else
        matrixStack.push_back(matrixStack.back() * transform.matrix);

    transform.traverse(*this);

    matrixStack.pop_back();
}

void ComputeBounds::apply(const vsg::Geometry& geometry)
{
    auto& arrayState = *arrayStateStack.back();
    arrayState.apply(geometry);

    if (geometry.indices) geometry.indices->accept(*this);

    for (auto& command : geometry.commands)
    {
        command->accept(*this);
    }
}

void ComputeBounds::apply(const vsg::VertexIndexDraw& vid)
{
    auto& arrayState = *arrayStateStack.back();
    arrayState.apply(vid);

    if (vid.indices) vid.indices->accept(*this);

    applyDrawIndexed(vid.firstIndex, vid.indexCount, vid.firstInstance, vid.instanceCount);
}

void ComputeBounds::apply(const vsg::BindVertexBuffers& bvb)
{
    arrayStateStack.back()->apply(bvb);
}

void ComputeBounds::apply(const BindIndexBuffer& bib)
{
    bib.indices->accept(*this);
}

void ComputeBounds::apply(const vsg::StateCommand& statecommand)
{
    statecommand.accept(*arrayStateStack.back());
}

void ComputeBounds::apply(const BufferInfo& bufferInfo)
{
    if (bufferInfo.data) bufferInfo.data->accept(*this);
}

void ComputeBounds::apply(const ushortArray& array)
{
    ushort_indices = &array;
    uint_indices = nullptr;
}

void ComputeBounds::apply(const uintArray& array)
{
    ushort_indices = nullptr;
    uint_indices = &array;
}

void ComputeBounds::apply(const Draw& draw)
{
    applyDraw(draw.firstVertex, draw.vertexCount, draw.firstInstance, draw.instanceCount);
}

void ComputeBounds::apply(const DrawIndexed& drawIndexed)
{
    applyDrawIndexed(drawIndexed.firstIndex, drawIndexed.indexCount, drawIndexed.firstInstance, drawIndexed.instanceCount);
};

void ComputeBounds::applyDraw(uint32_t firstVertex, uint32_t vertexCount, uint32_t firstInstance, uint32_t instanceCount)
{
    auto& arrayState = *arrayStateStack.back();
    uint32_t lastIndex = instanceCount > 1 ? (firstInstance + instanceCount) : firstInstance + 1;
    uint32_t endVertex = firstVertex + vertexCount;
    dmat4 matrix;
    if (!matrixStack.empty()) matrix = matrixStack.back();

    for (uint32_t instanceIndex = firstInstance; instanceIndex < lastIndex; ++instanceIndex)
    {
        if (auto vertices = arrayState.vertexArray(instanceIndex))
        {
            for (uint32_t i = firstVertex; i < endVertex; ++i)
            {
                bounds.add(matrix * dvec3(vertices->at(i)));
            }
        }
    }
}

void ComputeBounds::applyDrawIndexed(uint32_t firstIndex, uint32_t indexCount, uint32_t firstInstance, uint32_t instanceCount)
{
    auto& arrayState = *arrayStateStack.back();
    uint32_t lastIndex = instanceCount > 1 ? (firstInstance + instanceCount) : firstInstance + 1;
    uint32_t endIndex = firstIndex + indexCount;
    dmat4 matrix;
    if (!matrixStack.empty()) matrix = matrixStack.back();

    if (ushort_indices)
    {
        for (uint32_t instanceIndex = firstInstance; instanceIndex < lastIndex; ++instanceIndex)
        {
            if (auto vertices = arrayState.vertexArray(instanceIndex))
            {
                for (uint32_t i = firstIndex; i < endIndex; ++i)
                {
                    bounds.add(matrix * dvec3(vertices->at(ushort_indices->at(i))));
                }
            }
        }
    }
    else if (uint_indices)
    {
        for (uint32_t instanceIndex = firstInstance; instanceIndex < lastIndex; ++instanceIndex)
        {
            if (auto vertices = arrayState.vertexArray(instanceIndex))
            {
                for (uint32_t i = firstIndex; i < endIndex; ++i)
                {
                    bounds.add(matrix * dvec3(vertices->at(uint_indices->at(i))));
                }
            }
        }
    }
}

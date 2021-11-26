/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/commands/ClearAttachments.h>
#include <vsg/io/Options.h>
#include <vsg/state/StateGroup.h>
#include <vsg/viewer/View.h>
#include <vsg/viewer/WindowResizeHandler.h>
#include <vsg/vk/Context.h>
#include <vsg/vk/State.h>

#include <iostream>

using namespace vsg;

WindowResizeHandler::WindowResizeHandler()
{
}

void WindowResizeHandler::scale_rect(VkRect2D& rect)
{
    int32_t edge_x = rect.offset.x + static_cast<int32_t>(rect.extent.width);
    int32_t edge_y = rect.offset.y + static_cast<int32_t>(rect.extent.height);

    rect.offset.x = scale_parameter(rect.offset.x, previous_extent.width, new_extent.width);
    rect.offset.y = scale_parameter(rect.offset.y, previous_extent.height, new_extent.height);
    rect.extent.width = static_cast<uint32_t>(scale_parameter(edge_x, previous_extent.width, new_extent.width) - rect.offset.x);
    rect.extent.height = static_cast<uint32_t>(scale_parameter(edge_y, previous_extent.height, new_extent.height) - rect.offset.y);
}

bool WindowResizeHandler::visit(Object* object)
{
    if (visited.count(object) != 0) return false;
    visited.insert(object);
    return true;
}

void WindowResizeHandler::apply(vsg::BindGraphicsPipeline& bindPipeline)
{
    GraphicsPipeline* graphicsPipeline = bindPipeline.pipeline;

    if (!visit(graphicsPipeline)) return;

    if (graphicsPipeline)
    {
        struct ContainsViewport : public ConstVisitor
        {
            bool foundViewport = false;
            void apply(const ViewportState&) { foundViewport = true; }
            bool operator()(const GraphicsPipeline& gp)
            {
                for (auto& pipelineState : gp.pipelineStates)
                {
                    pipelineState->accept(*this);
                }
                return foundViewport;
            }
        } containsViewport;

        bool needToRegenerateGraphicsPipeline = !containsViewport(*graphicsPipeline);
        if (needToRegenerateGraphicsPipeline)
        {
            graphicsPipeline->release(context->viewID);
            graphicsPipeline->compile(*context);
        }
    }
}

void WindowResizeHandler::apply(vsg::Object& object)
{
    object.traverse(*this);
}

void WindowResizeHandler::apply(vsg::StateGroup& sg)
{
    if (!visit(&sg)) return;

    for (auto& command : sg.getStateCommands())
    {
        command->accept(*this);
    }
    sg.traverse(*this);
}

void WindowResizeHandler::apply(ClearAttachments& clearAttachments)
{
    if (!visit(&clearAttachments)) return;

    for (auto& clearRect : clearAttachments.rects)
    {
        auto& rect = clearRect.rect;
        scale_rect(rect);
    }
}

void WindowResizeHandler::apply(vsg::View& view)
{
    if (!visit(&view)) return;

    context->viewID = view.viewID;

    if (!view.camera)
    {
        view.traverse(*this);
        return;
    }

    view.camera->getProjectionMatrix()->changeExtent(previous_extent, new_extent);

    auto viewportState = view.camera->getViewportState();

    size_t num_viewports = std::min(viewportState->viewports.size(), viewportState->scissors.size());
    for (size_t i = 0; i < num_viewports; ++i)
    {
        auto& viewport = viewportState->viewports[i];
        auto& scissor = viewportState->scissors[i];

        bool renderAreaMatches = (renderArea.offset.x == scissor.offset.x) && (renderArea.offset.y == scissor.offset.y) &&
                                 (renderArea.extent.width == scissor.extent.width) && (renderArea.extent.height == scissor.extent.height);

        scale_rect(scissor);

        viewport.x = static_cast<float>(scissor.offset.x);
        viewport.y = static_cast<float>(scissor.offset.y);
        viewport.width = static_cast<float>(scissor.extent.width);
        viewport.height = static_cast<float>(scissor.extent.height);

        if (renderAreaMatches)
        {
            renderArea = scissor;
        }
    }

    context->defaultPipelineStates.emplace_back(viewportState);

    view.traverse(*this);

    context->defaultPipelineStates.pop_back();
}

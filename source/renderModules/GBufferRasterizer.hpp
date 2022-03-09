#pragma once
#include <buffers/GBuffer.hpp>

#include <vsg/all.h>

#include <cstdint>

class GBufferRasterizer
{
public:
    GBufferRasterizer(
        vsg::Device* device, uint32_t width, uint32_t height, bool double_sided = false, bool blend = false);

    void compile(vsg::Context& context) const;

    void update_image_layouts(vsg::Context& context) const;

protected:
    void setup_graphics_pipeline();

    uint32_t _width, _height;
    bool _double_sided, _blend;
    vsg::Device* _device;

    vsg::ref_ptr<GBuffer> _g_buffer;
    vsg::ref_ptr<vsg::BindGraphicsPipeline> _bind_graphics_pipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> _bind_descriptor_set;
    vsg::ref_ptr<vsg::RenderPass> _render_pass;
    vsg::ref_ptr<vsg::Framebuffer> _frame_buffer;
    vsg::ref_ptr<vsg::ImageView> _depth;
    vsg::ref_ptr<vsg::RenderGraph> _render_graph;
};
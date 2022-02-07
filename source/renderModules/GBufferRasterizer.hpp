#pragma once
#include <buffers/GBuffer.hpp>

#include <vsg/all.h>

#include <cstdint>

class GBufferRasterizer
{
public:
    GBufferRasterizer(
        vsg::Device* device, uint32_t width, uint32_t height, bool double_sided = false, bool blend = false);

    void compile(vsg::Context& context);

    void update_image_layouts(vsg::Context& context);

    vsg::ref_ptr<GBuffer> g_buffer;
    vsg::ref_ptr<vsg::BindGraphicsPipeline> bind_graphics_pipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> bind_descriptor_set;
    vsg::ref_ptr<vsg::RenderPass> render_pass;
    vsg::ref_ptr<vsg::Framebuffer> frame_buffer;
    vsg::ref_ptr<vsg::ImageView> depth;
    vsg::ref_ptr<vsg::RenderGraph> render_graph;

protected:
    uint32_t _width, _height;
    bool _double_sided, _blend;
    vsg::Device* _device;

    void setup_graphics_pipeline();
};
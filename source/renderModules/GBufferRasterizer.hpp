#pragma once
#include <buffers/GBuffer.hpp>

#include <vsg/all.h>

#include <cstdint>

class GBufferRasterizer{
public:
    GBufferRasterizer(vsg::Device* device, uint32_t width, uint32_t height, bool doubleSided = false, bool blend = false);

    void compile(vsg::Context& context);

    void updateImageLayouts(vsg::Context& context);

    vsg::ref_ptr<GBuffer> gBuffer;
    vsg::ref_ptr<vsg::BindGraphicsPipeline> bindGraphicsPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;
    vsg::ref_ptr<vsg::RenderPass> renderPass;
    vsg::ref_ptr<vsg::Framebuffer> frameBuffer;
    vsg::ref_ptr<vsg::ImageView> depth;
    vsg::ref_ptr<vsg::RenderGraph> renderGraph;
protected:
    uint32_t width, height;
    bool doubleSided, blend;
    vsg::Device* device;

    void setupGraphicsPipeline();
};
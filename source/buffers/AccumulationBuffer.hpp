#pragma once

#include "GBuffer.hpp"
#include "IlluminationBuffer.hpp"

#include <vsg/all.h>

// class to holding buffer needed for accumulation. These are:
// prevIllu, prevDepth, prevNormal, spp, prevSpp, motion
class AccumulationBuffer : public vsg::Inherit<vsg::Object, AccumulationBuffer>
{
public:
    AccumulationBuffer(uint32_t width, uint32_t height);

    vsg::ref_ptr<vsg::DescriptorImage> prev_illu, prev_illu_squared, prev_depth, prev_normal, spp, prev_spp, motion;

    void update_descriptor(vsg::BindDescriptorSet* desc_set, const vsg::BindingMap& binding_map);

    void update_image_layouts(vsg::Context& context);

    void compile(vsg::Context& context);

    void copy_to_back_images(vsg::ref_ptr<vsg::Commands> commands, vsg::ref_ptr<GBuffer> g_buffer,
        vsg::ref_ptr<IlluminationBuffer> illumination_buffer);

protected:
    uint32_t _width, _height;

    void setup_images();
};

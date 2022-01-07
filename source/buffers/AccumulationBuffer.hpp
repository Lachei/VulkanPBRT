#pragma once

#include "GBuffer.hpp"
#include "IlluminationBuffer.hpp"

#include <vsg/all.h>

// class to holding buffer needed for accumulation. These are:
// prevIllu, prevDepth, prevNormal, spp, prevSpp, motion
class AccumulationBuffer: public vsg::Inherit<vsg::Object, AccumulationBuffer>{
public:
    AccumulationBuffer(uint32_t width, uint32_t height);

    vsg::ref_ptr<vsg::DescriptorImage> prevIllu, prevIlluSquared, prevDepth, prevNormal, spp, prevSpp, motion;

    void updateDescriptor(vsg::BindDescriptorSet* descSet, const vsg::BindingMap& bindingMap);

    void updateImageLayouts(vsg::Context& context);

    void compile(vsg::Context& context);

    void copyToBackImages(vsg::ref_ptr<vsg::Commands> commands, vsg::ref_ptr<GBuffer> gBuffer,
                          vsg::ref_ptr<IlluminationBuffer> illuminationBuffer);

protected:
    uint32_t width, height;

    void setupImages();
};

#pragma once
#include "../TAA/taa.hpp"
#include "../AccumulationBuffer.hpp"
#include "../GBuffer.hpp"
#include "../IlluminationBuffer.hpp"


#include <vsg/all.h>

#include "vsg/core/ref_ptr.h"

class BFR: public vsg::Inherit<vsg::Object, BFR>{
public:
    uint32_t depthBinding = 0, normalBinding = 1, materialBinding = 2, albedoBinding = 3, prevDepthBinding = -1, prevNormalBinding = -1, motionBinding = 4, sampleBinding = 5, sampledDenIlluBinding = 6, denoisedIlluBinding = 9, finalBinding = 7, noisyBinding = 8, denoisedBinding = 9;

    BFR(uint32_t width, uint32_t height, uint32_t workWidth, uint32_t workHeight, vsg::ref_ptr<GBuffer> gBuffer,
        vsg::ref_ptr<IlluminationBuffer> illuBuffer, vsg::ref_ptr<AccumulationBuffer> accBuffer);

    void addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph, vsg::ref_ptr<vsg::PushConstants> pushConstants);

    void compile(vsg::Context& context);

    void updateImageLayouts(vsg::Context& context);

    vsg::ref_ptr<vsg::ComputePipeline> bfrPipeline;
    vsg::ref_ptr<vsg::BindComputePipeline> bindBfrPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;

    vsg::ref_ptr<vsg::DescriptorImage> accumulatedIllumination, sampledAccIllu, finalIllumination;
    vsg::ref_ptr<Taa> taaPipeline;

    vsg::ref_ptr<vsg::Sampler> sampler;
protected:
    uint32_t width, height, workWidth, workHeight;
    vsg::ref_ptr<GBuffer> gBuffer;
};

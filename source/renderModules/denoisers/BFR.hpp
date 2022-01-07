#pragma once
#include <buffers/AccumulationBuffer.hpp>
#include <buffers/GBuffer.hpp>
#include <buffers/IlluminationBuffer.hpp>

#include <vsg/all.h>

class BFR: public vsg::Inherit<vsg::Object, BFR>{
public:
    BFR(uint32_t width, uint32_t height, uint32_t workWidth, uint32_t workHeight, vsg::ref_ptr<GBuffer> gBuffer,
        vsg::ref_ptr<IlluminationBuffer> illuBuffer, vsg::ref_ptr<AccumulationBuffer> accBuffer);

    void compile(vsg::Context& context);
    void updateImageLayouts(vsg::Context& context);
    void addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph, vsg::ref_ptr<vsg::PushConstants> pushConstants);
    vsg::ref_ptr<vsg::DescriptorImage> getFinalDescriptorImage() const;
private:
    uint32_t width, height, workWidth, workHeight;
    vsg::ref_ptr<GBuffer> gBuffer;

    uint32_t depthBinding = 0;
    uint32_t normalBinding = 1;
    uint32_t materialBinding = 2;
    uint32_t albedoBinding = 3;
    uint32_t prevDepthBinding = -1;
    uint32_t prevNormalBinding = -1;
    uint32_t motionBinding = 4;
    uint32_t sampleBinding = 5;
    uint32_t sampledDenIlluBinding = 6;
    uint32_t denoisedIlluBinding = 9;
    uint32_t finalBinding = 7;
    uint32_t noisyBinding = 8;
    uint32_t denoisedBinding = 9;

    vsg::ref_ptr<vsg::ComputePipeline> bfrPipeline;
    vsg::ref_ptr<vsg::BindComputePipeline> bindBfrPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;

    vsg::ref_ptr<vsg::DescriptorImage> accumulatedIllumination, sampledAccIllu, finalIllumination;

    vsg::ref_ptr<vsg::Sampler> sampler;
};

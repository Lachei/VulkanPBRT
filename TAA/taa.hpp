#pragma once
#include "../Defines.hpp"
#include "../GBuffer.hpp"
#include "../AccumulationBuffer.hpp"
#include "../PipelineStructs.hpp"

#include <vsg/all.h>




class Taa: public vsg::Inherit<vsg::Object, Taa>{
public:
    uint denoisedBinding = 1, motionBinding = 0, finalImageBinding = 2, accumulationBinding = 3;

    Taa(uint width, uint height, uint workWidth, uint workHeight, vsg::ref_ptr<GBuffer> gBuffer, vsg::ref_ptr<AccumulationBuffer> accBuffer,
        vsg::ref_ptr<vsg::DescriptorImage> denoised);

    void addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph);

    void compile(vsg::Context& context);

    void updateImageLayouts(vsg::Context& context);

    void copyFinalImageToAccumulation(vsg::ref_ptr<vsg::Commands> commands);

    void copyFinalImage(vsg::ref_ptr<vsg::Commands> commands, vsg::ref_ptr<vsg::Image> dstImage);

    vsg::ref_ptr<vsg::ComputePipeline> pipeline;
    vsg::ref_ptr<vsg::BindComputePipeline> bindPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;
    vsg::ref_ptr<vsg::DescriptorImage> finalImage, accumulationImage;

    vsg::ref_ptr<vsg::Sampler> sampler;
protected:
    uint width, height, workWidth, workHeight;
};



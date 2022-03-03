#pragma once

#include <renderModules/Taa.hpp>
#include <buffers/IlluminationBuffer.hpp>

#include <vsg/all.h>

// Reimplementation of the Blockwise multi order feature regression method presented in https://webpages.tuni.fi/foi/papers/Koskela-TOG-2019-Blockwise_Multi_Order_Feature_Regression_for_Real_Time_Path_Tracing_Reconstruction.pdf
// This version is converted to vulkan and optimized, such that it uses less kernels, and does require accumulated images to be given to it.
// Further the spacial features used for feature fitting are in screen space to reduce calculation efforts.


class BMFR: public vsg::Inherit<vsg::Object, BMFR>{
public:
    BMFR(uint32_t width, uint32_t height, uint32_t workWidth, uint32_t workHeight, vsg::ref_ptr<GBuffer> gBuffer,
         vsg::ref_ptr<IlluminationBuffer> illuBuffer, vsg::ref_ptr<AccumulationBuffer> accBuffer, uint32_t fittingKernel = 256);

    void compile(vsg::Context& context);
    void updateImageLayouts(vsg::Context& context);
    void addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph, vsg::ref_ptr<vsg::PushConstants> pushConstants);
    vsg::ref_ptr<vsg::DescriptorImage> getFinalDescriptorImage() const;
private:
    uint32_t depthBinding = 0, normalBinding = 1, materialBinding = 2, albedoBinding = 3, motionBinding = 4, sampleBinding = 5, sampledDenIlluBinding = 6, finalBinding = 7, noisyBinding = 8, denoisedBinding = 9, featureBufferBinding = 10, weightsBinding = 11;
    uint32_t amtOfFeatures = 13;

    uint32_t width, height, workWidth, workHeight, fittingKernel, widthPadded, heightPadded;
    vsg::ref_ptr<GBuffer> gBuffer;
    vsg::ref_ptr<vsg::Sampler> sampler;
    vsg::ref_ptr<vsg::BindComputePipeline> bindPrePipeline, bindFitPipeline, bindPostPipeline;
    vsg::ref_ptr<vsg::ComputePipeline> bmfrPrePipeline, bmfrFitPipeline, bmfrPostPipeline;
    vsg::ref_ptr<vsg::DescriptorImage> accumulatedIllumination, finalIllumination, featureBuffer, rMat, weights;
    vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;
};


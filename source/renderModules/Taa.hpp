#pragma once
#include <buffers/GBuffer.hpp>
#include <buffers/AccumulationBuffer.hpp>

#include <vsg/all.h>

#include <cstdint>


class Taa: public vsg::Inherit<vsg::Object, Taa>{
public:
    uint32_t denoisedBinding = 1, motionBinding = 0, finalImageBinding = 2, accumulationBinding = 3;

    Taa(uint32_t width, uint32_t height, uint32_t workWidth, uint32_t workHeight, vsg::ref_ptr<GBuffer> gBuffer, vsg::ref_ptr<AccumulationBuffer> accBuffer,
        vsg::ref_ptr<vsg::DescriptorImage> denoised);

    void compile(vsg::Context& context);
    void updateImageLayouts(vsg::Context& context);
    void addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph);
    vsg::ref_ptr<vsg::DescriptorImage> getFinalDescriptorImage() const;
private:
    void copyFinalImage(vsg::ref_ptr<vsg::Commands> commands, vsg::ref_ptr<vsg::Image> dstImage);

    uint32_t width, height, workWidth, workHeight;

    vsg::ref_ptr<vsg::ComputePipeline> pipeline;
    vsg::ref_ptr<vsg::BindComputePipeline> bindPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;
    vsg::ref_ptr<vsg::DescriptorImage> finalImage, accumulationImage;

    vsg::ref_ptr<vsg::Sampler> sampler;
};
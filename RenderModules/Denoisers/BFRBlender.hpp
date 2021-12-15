#pragma once
#include <vsg/all.h>

#include <cstdint>

class BFRBlender: public vsg::Inherit<vsg::Object, BFRBlender>{
public:
    BFRBlender(uint32_t width, uint32_t height,
               vsg::ref_ptr<vsg::DescriptorImage> averageImage, vsg::ref_ptr<vsg::DescriptorImage> averageSquaredImage,
               vsg::ref_ptr<vsg::DescriptorImage> denoised0, vsg::ref_ptr<vsg::DescriptorImage> denoised1,
               vsg::ref_ptr<vsg::DescriptorImage> denoised2,
               uint32_t workWidth = 16, uint32_t workHeight = 16, uint32_t filterRadius = 2);

    void compile(vsg::Context& context);
    void updateImageLayouts(vsg::Context& context);
    void addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph);
    void copyFinalImage(vsg::ref_ptr<vsg::Commands> commands, vsg::ref_ptr<vsg::Image> dstImage);
    vsg::ref_ptr<vsg::DescriptorImage> getFinalDescriptorImage() const;
private:
    uint32_t averageBinding = 0, averageSquaredBinding = 1, denoised0Binding = 2, denoised1Binding = 3, denoised2Binding = 4, finalBinding = 5;
    vsg::ref_ptr<vsg::DescriptorImage> finalImage;
    vsg::ref_ptr<vsg::ComputePipeline> pipeline;
    vsg::ref_ptr<vsg::BindComputePipeline> bindPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;
    uint32_t width, height, workWidth, workHeight, filterRadius;
};

#pragma once
#include <vsg/all.h>
#include "../RenderIO.hpp"
#include "../GBuffer.hpp"
#include "../IlluminationBuffer.hpp"
#include "../AccumulationBuffer.hpp"

class Accumulator : public vsg::Inherit<vsg::Object, Accumulator>
{
private:
    int width, height;
public:
    // Always takes the first image in the illumination buffer and accumulates it
    Accumulator(vsg::ref_ptr<GBuffer> gBuffer, vsg::ref_ptr<IlluminationBuffer> illuminationBuffer, DoubleMatrices &matrices, int workWidth = 16, int workHeight = 16);

    void compileImages(vsg::Context &context);
    void updateImageLayouts(vsg::Context &context);
    void addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph);
    // Frameindex is needed to upload the correct matrix
    void setFrameIndex(int frame);

    vsg::ref_ptr<IlluminationBuffer> accumulatedIllumination;
    vsg::ref_ptr<AccumulationBuffer> accumulationBuffer;

private:
    struct PushConstants
    {
        vsg::mat4 view, invView, prevView;
        vsg::vec4 prevPos;
        int frameNumber;
    };
    class PCValue : public vsg::Inherit<vsg::Value<PushConstants>, PCValue>
    {
    public:
        PCValue(){}
    };
    std::string shaderPath = "shaders/accumulator.comp.spv";
    int workWidth, workHeight;
    vsg::ref_ptr<GBuffer> gBuffer;
    vsg::ref_ptr<IlluminationBuffer> originalIllumination;
    DoubleMatrices matrices;
    int frameIndex = 0;
    vsg::ref_ptr<vsg::BindComputePipeline> bindPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;
    vsg::ref_ptr<vsg::PushConstants> pushConstants;
    vsg::ref_ptr<PCValue> pushConstantsValue;
};
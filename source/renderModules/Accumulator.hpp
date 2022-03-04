#pragma once
#include <vsg/all.h>
#include <io/RenderIO.hpp>
#include <buffers/GBuffer.hpp>
#include <buffers/IlluminationBuffer.hpp>
#include <buffers/AccumulationBuffer.hpp>

class Accumulator : public vsg::Inherit<vsg::Object, Accumulator>
{
private:
    int width, height;
public:
    // Always takes the first image in the illumination buffer and accumulates it
    Accumulator(vsg::ref_ptr<GBuffer> gBuffer, vsg::ref_ptr<IlluminationBuffer> illuminationBuffer, bool separateMatrices, int workWidth = 16, int workHeight = 16);

    void compileImages(vsg::Context &context);
    void updateImageLayouts(vsg::Context &context);
    void addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph);
    // Frameindex is needed to upload the correct matrix
    void setCameraMatrices(int frameIndex, const CameraMatrices& cur, const CameraMatrices& prev);

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
    std::string shaderPath = "shaders/accumulator.comp";    //normal glsl file has to be loaded as the shader has to be adopted to separateMatrices style
    int workWidth, workHeight;
    vsg::ref_ptr<GBuffer> gBuffer;
    vsg::ref_ptr<IlluminationBuffer> originalIllumination;
    vsg::ref_ptr<vsg::BindComputePipeline> bindPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;
    vsg::ref_ptr<vsg::PushConstants> pushConstants;
    vsg::ref_ptr<PCValue> pushConstantsValue;
    bool _separateMatrices;
};
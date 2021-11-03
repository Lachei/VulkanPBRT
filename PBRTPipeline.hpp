#pragma once

#include "GBuffer.hpp"
#include "IlluminationBuffer.hpp"
#include "RayTracingVisitor.hpp"
#include "AccumulationBuffer.hpp"

#include <vsg/all.h>
#include <vsgXchange/glsl.h>

#include <cstdint>

enum class IlluminationBufferType
{
    FINAL,
    DEMODULATED,
    FINAL_DEMODULATED,
    FINAL_DIRECT_INDIRECT,
};

enum class RayTracingRayOrigin
{
    CAMERA,
    GBUFFER,
};

class PBRTPipeline : public vsg::Inherit<vsg::Object, PBRTPipeline>
{
public:
    PBRTPipeline(uint32_t width, uint32_t height, uint32_t maxRecursionDepth, vsg::Node* scene, vsg::ref_ptr<GBuffer> gBuffer, vsg::ref_ptr<AccumulationBuffer> accumulationBuffer,
                 IlluminationBufferType illuminationBufferType, bool writeGBuffer, RayTracingRayOrigin rayTracingRayOrigin);

    void setTlas(vsg::ref_ptr<vsg::AccelerationStructure> as);
    void compile(vsg::Context& context);
    void updateImageLayouts(vsg::Context& context);
    void addTraceRaysToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph, vsg::ref_ptr<vsg::PushConstants> pushConstants);
    vsg::ref_ptr<IlluminationBuffer> getIlluminationBuffer() const;
private:
    void setupPipeline(vsg::Node* scene, bool useExternalGBuffer, IlluminationBufferType illuminationBufferType);
    vsg::ref_ptr<vsg::ShaderStage> setupRaygenShader(std::string raygenPath, bool useExternalGBuffer, IlluminationBufferType illuminationBufferType);

    std::vector<bool> opaqueGeometries;
    uint32_t width, height, maxRecursionDepth, samplePerPixel;

    // TODO: add buffers here
    vsg::ref_ptr<GBuffer> gBuffer;
    vsg::ref_ptr<AccumulationBuffer> accumulationBuffer;
    vsg::ref_ptr<IlluminationBuffer> illuminationBuffer;

    //resources which have to be added as childs to a scenegraph for rendering
    vsg::ref_ptr<vsg::BindRayTracingPipeline> bindRayTracingPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> bindRayTracingDescriptorSet;
    vsg::ref_ptr<vsg::PushConstants> pushConstants;

    //shader binding table for trace rays
    vsg::ref_ptr<vsg::RayTracingShaderBindingTable> shaderBindingTable;

    //binding map containing all descriptor bindings in the shaders
    vsg::BindingMap bindingMap;
};

#pragma once

#include "GBuffer.hpp"
#include "IlluminationBuffer.hpp"
#include "RayTracingVisitor.hpp"
#include "AccumulationBuffer.hpp"

#include <vsg/all.h>
#include <vsgXchange/glsl.h>

#include <cstdint>

class PBRTPipeline: public vsg::Inherit<vsg::Object, PBRTPipeline>{
public:
    struct ConstantInfos{
        uint32_t lightCount;
        uint32_t minRecursionDepth;
        uint32_t maxRecursionDepth;
    };

    PBRTPipeline(uint32_t width, uint32_t height, uint32_t maxRecursionDepth, vsg::Node* scene,
                 vsg::ref_ptr<IlluminationBuffer> illuminationBuffer);
    PBRTPipeline(uint32_t width, uint32_t height, uint32_t maxRecursionDepth, vsg::Node* scene,
                 vsg::ref_ptr<IlluminationBuffer> illuminationBuffer, vsg::ref_ptr<GBuffer> gBuffer);

    void compile(vsg::Context& context);

    void setTlas(vsg::ref_ptr<vsg::AccelerationStructure> as);

    void updateImageLayouts(vsg::Context& context);

    void cmdCopyToAccImages(vsg::ref_ptr<vsg::Commands> commands);

    uint32_t width, height, maxRecursionDepth, samplePerPixel;
    vsg::ref_ptr<GBuffer> gBuffer;
    vsg::ref_ptr<IlluminationBuffer> illuminationBuffer;        //is retrieved in the constructor
    vsg::ref_ptr<AccumulationBuffer> accumulationBuffer;

    //resources which have to be added as childs to a scenegraph for rendering
    vsg::ref_ptr<vsg::BindRayTracingPipeline> bindRayTracingPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> bindRayTracingDescriptorSet;
    vsg::ref_ptr<vsg::PushConstants> pushConstants;

    //shader binding table for trace rays
    vsg::ref_ptr<vsg::RayTracingShaderBindingTable> shaderBindingTable;

    //binding map containing all descriptor bindings in the shaders
    vsg::BindingMap bindingMap;
protected:
    std::vector<bool> opaqueGeometries;

    class ConstantInfosValue : public vsg::Inherit<vsg::Value<ConstantInfos>, ConstantInfosValue>{
        public:
        ConstantInfosValue() {}
    };

    bool useExternalGBuffer;

    //handles setup of the raygen shader.
    //curently does nothing as only one raygen shader exists
    vsg::ref_ptr<vsg::ShaderStage> setupRaygenShader(std::string raygenPath);

    void setupPipeline(vsg::Node* scene);
};

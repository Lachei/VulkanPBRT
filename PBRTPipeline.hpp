#pragma once

#include <vsg/all.h>
#include "GBuffer.hpp"
#include "IlluminationBuffer.hpp"
#include "RayTracingVisitor.hpp"

class PBRTPipeline: public vsg::Inherit<vsg::Object, PBRTPipeline>{
public:
    struct ConstantInfos{
        uint lightCount;
        uint minRecursionDepth;
        uint maxRecursionDepth;
    };

    PBRTPipeline(uint width, uint height, uint maxRecursionDepth, vsg::Node* scene): width(width), height(height), maxRecursionDepth(maxRecursionDepth){
        //GBuffer setup
        useExternalGBuffer = false;
        gBuffer = GBuffer::create(width, height);

        setupPipeline(scene);
    }
    PBRTPipeline(uint width, uint height, uint maxRecursionDepth, vsg::Node* scene, vsg::ref_ptr<GBuffer> gBuffer): width(width), height(height), maxRecursionDepth(maxRecursionDepth), gBuffer(gBuffer){
        useExternalGBuffer = true;

        setupPipeline(scene);
    }

    void compile(vsg::Context& context){
        gBuffer->compile(context);
        illuminationBuffer->compile(context);
    }

    //automatically sets everything in the rayTracing binder concerning the illumination buffer
    void setIlluminationBuffer(vsg::ref_ptr<IlluminationBuffer> buffer){
        buffer->updateDescriptor(bindRayTracingDescriptorSet);
        illuminationBuffer = buffer;

        //set different raygen shaders according to state of external gbuffer and illumination buffer type
        //TODO: create different shaders for correct model
        std::string raygenPath = "shaders/raygen.rgen.spv";
        if(useExternalGBuffer){
            auto final = illuminationBuffer.cast<IlluminationBufferFinal>();
            auto finalDirIndir = illuminationBuffer.cast<IlluminationBufferFinalDirIndir>();
            auto finalDemod = illuminationBuffer.cast<IlluminationBufferFinalDemodulated>();
            if(final){
                raygenPath = "shaders/raygen.rgen.spv";
            }
            else if(finalDirIndir){
                raygenPath = "shaders/raygen.rgen.spv";
            }
            else{
                raygenPath = "shaders/raygen.rgen.spv";
            }
        }
        else{
            auto final = illuminationBuffer.cast<IlluminationBufferFinal>();
            auto finalDirIndir = illuminationBuffer.cast<IlluminationBufferFinalDirIndir>();
            auto finalDemod = illuminationBuffer.cast<IlluminationBufferFinalDemodulated>();
            if(final){
                raygenPath = "shaders/raygen.rgen.spv";
            }
            else if(finalDirIndir){
                raygenPath = "shaders/raygen.rgen.spv";
            }
            else{
                raygenPath = "shaders/raygen.rgen.spv";
            }
        }
        raygenShader->module = vsg::ShaderModule::read(raygenPath);
    }

    void setTlas(vsg::ref_ptr<vsg::AccelerationStructure> as){
        auto accelDescriptor = vsg::DescriptorAccelerationStructure::create(vsg::AccelerationStructures{as}, 0, 0);
        bindRayTracingDescriptorSet->descriptorSet->descriptors.push_back(accelDescriptor);
    }

    uint width, height, maxRecursionDepth, samplePerPixel;
    vsg::ref_ptr<GBuffer> gBuffer;
    vsg::ref_ptr<IlluminationBuffer> illuminationBuffer;        //has to be set externally
    vsg::ref_ptr<vsg::ShaderStage> raygenShader;

    //resources which have to be added as childs to a scenegraph for rendering
    vsg::ref_ptr<vsg::BindRayTracingPipeline> bindRayTracingPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> bindRayTracingDescriptorSet;
    vsg::ref_ptr<vsg::PushConstants> pushConstants;
    vsg::ref_ptr<vsg::TraceRays> traceRays;

    //shader binding table for trace rays
    vsg::ref_ptr<vsg::RayTracingShaderBindingTable> shaderBindingTable;

protected:
    class ConstantInfosValue : public vsg::Inherit<vsg::Value<ConstantInfos>, ConstantInfosValue>{
        public:
        ConstantInfosValue(){}
    };

    bool useExternalGBuffer;

    void setupPipeline(vsg::Node* scene){
        //parsing data from scene
        CreateRayTracingDescriptorTraversal buildDescriptorBinding;
        scene->accept(buildDescriptorBinding);
        bindRayTracingDescriptorSet = buildDescriptorBinding.getBindDescriptorSet();
        auto rayTracingPipelineLayout = buildDescriptorBinding.pipelineLayout;

        //creating the constant infos uniform buffer object
        auto constantInfos = ConstantInfosValue::create();
        constantInfos->value().lightCount = buildDescriptorBinding.packedLights.size();
        constantInfos->value().maxRecursionDepth = maxRecursionDepth;
        constantInfos->value().minRecursionDepth = maxRecursionDepth;
        auto constantInfosDescriptor = vsg::DescriptorBuffer::create(constantInfos, 19, 0);
        bindRayTracingDescriptorSet->descriptorSet->descriptors.push_back(constantInfosDescriptor);
        rayTracingPipelineLayout->setLayouts[0]->bindings.push_back({19, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr});

        //creating the shader stages and shader binding table
        std::string raygenPath = "shaders/raygen.rgen.spv";     //default reaygen shader
        std::string raymissPath = "shaders/miss.rmiss.spv";
        std::string shadowMissPath = "shaders/shadow.rmiss.spv";
        std::string closesthitPath = "shaders/pbr_closesthit.rchit.spv";

        raygenShader = vsg::ShaderStage::read(VK_SHADER_STAGE_RAYGEN_BIT_KHR, "main", raygenPath);
        auto raymissShader = vsg::ShaderStage::read(VK_SHADER_STAGE_MISS_BIT_KHR, "main", raymissPath);
        auto shadowMissShader = vsg::ShaderStage::read(VK_SHADER_STAGE_MISS_BIT_KHR, "main", shadowMissPath);
        auto closesthitShader = vsg::ShaderStage::read(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "main", closesthitPath);
        if(!raygenShader || !raymissShader || !closesthitShader || !shadowMissShader){
            throw vsg::Exception{"Error: vcreate PBRTPipeline(...) failed to create shader stages."};
        }
        auto shaderStage = vsg::ShaderStages{raygenShader, raymissShader, shadowMissShader, closesthitShader};
        auto raygenShaderGroup = vsg::RayTracingShaderGroup::create();
        raygenShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        raygenShaderGroup->generalShader = 0;
        auto raymissShaderGroup = vsg::RayTracingShaderGroup::create();
        raymissShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        raymissShaderGroup->generalShader = 1;
        auto shadowMissShaderGroup = vsg::RayTracingShaderGroup::create();
        shadowMissShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        shadowMissShaderGroup->generalShader = 2;
        auto closesthitShaderGroup = vsg::RayTracingShaderGroup::create();
        closesthitShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        closesthitShaderGroup->closestHitShader = 3;
        auto shaderGroups = vsg::RayTracingShaderGroups{raygenShaderGroup, raymissShaderGroup, shadowMissShaderGroup, closesthitShaderGroup};
        shaderBindingTable = vsg::RayTracingShaderBindingTable::create();
        shaderBindingTable->bindingTableEntries.raygenGroups = {raygenShaderGroup};
        shaderBindingTable->bindingTableEntries.raymissGroups = {raymissShaderGroup, shadowMissShaderGroup};
        shaderBindingTable->bindingTableEntries.hitGroups = {closesthitShaderGroup};
        auto pipeline = vsg::RayTracingPipeline::create(rayTracingPipelineLayout, shaderStage, shaderGroups, shaderBindingTable, 2 * maxRecursionDepth);
        bindRayTracingPipeline = vsg::BindRayTracingPipeline::create(pipeline);

        //adding gbuffer bindings to the descriptor
        gBuffer->updateDescriptor(bindRayTracingDescriptorSet);
    }
};
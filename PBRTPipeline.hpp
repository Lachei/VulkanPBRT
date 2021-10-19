#pragma once

#include "Defines.hpp"
#include "GBuffer.hpp"
#include "IlluminationBuffer.hpp"
#include "RayTracingVisitor.hpp"
#include "AccumulationBuffer.hpp"

#include <vsg/all.h>
#include <vsgXchange/glsl.h>

class PBRTPipeline: public vsg::Inherit<vsg::Object, PBRTPipeline>{
public:
    struct ConstantInfos{
        uint32_t lightCount;
        uint32_t minRecursionDepth;
        uint32_t maxRecursionDepth;
    };

    PBRTPipeline(uint32_t width, uint32_t height, uint32_t maxRecursionDepth, vsg::Node* scene, vsg::ref_ptr<IlluminationBuffer> illuminationBuffer): 
    width(width), height(height), maxRecursionDepth(maxRecursionDepth), illuminationBuffer(illuminationBuffer){
        //GBuffer setup
        useExternalGBuffer = false;
        if(!illuminationBuffer.cast<IlluminationBufferFinal>())
            gBuffer = GBuffer::create(width, height);

        setupPipeline(scene);
    }
    PBRTPipeline(uint32_t width, uint32_t height, uint32_t maxRecursionDepth, vsg::Node* scene, vsg::ref_ptr<IlluminationBuffer> illuminationBuffer, vsg::ref_ptr<GBuffer> gBuffer): 
    width(width), height(height), maxRecursionDepth(maxRecursionDepth), illuminationBuffer(illuminationBuffer), gBuffer(gBuffer){
        useExternalGBuffer = true;

        setupPipeline(scene);
    }

    void compile(vsg::Context& context){
        illuminationBuffer->compile(context);
        if(gBuffer)
            gBuffer->compile(context);
        if(accumulationBuffer)
            accumulationBuffer->compile(context);
    }

    void setTlas(vsg::ref_ptr<vsg::AccelerationStructure> as){
        auto tlas = as.cast<vsg::TopLevelAccelerationStructure>();
        assert(tlas);
        for(int i = 0; i < tlas->geometryInstances.size(); ++i){
            if(opaqueGeometries[i])
                tlas->geometryInstances[i]->shaderOffset = 0;
            else
                tlas->geometryInstances[i]->shaderOffset = 1;
                tlas->geometryInstances[i]->geometryFlags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
        }
        auto accelDescriptor = vsg::DescriptorAccelerationStructure::create(vsg::AccelerationStructures{as}, 0, 0);
        bindRayTracingDescriptorSet->descriptorSet->descriptors.push_back(accelDescriptor);
    }

    void updateImageLayouts(vsg::Context& context){
        illuminationBuffer->updateImageLayouts(context);
        if(gBuffer)
            gBuffer->updateImageLayouts(context);
        if(accumulationBuffer)
            accumulationBuffer->updateImageLayouts(context);
    }

    void cmdCopyToAccImages(vsg::ref_ptr<vsg::Commands> commands){
        if(accumulationBuffer)
            accumulationBuffer->copyToBackImages(commands, gBuffer, illuminationBuffer);
    }

    uint32_t width, height, maxRecursionDepth, samplePerPixel;
    vsg::ref_ptr<GBuffer> gBuffer;
    vsg::ref_ptr<IlluminationBuffer> illuminationBuffer;        //is retrieved in the constructor
    vsg::ref_ptr<AccumulationBuffer> accumulationBuffer;

    //resources which have to be added as childs to a scenegraph for rendering
    vsg::ref_ptr<vsg::BindRayTracingPipeline> bindRayTracingPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> bindRayTracingDescriptorSet;
    vsg::ref_ptr<vsg::PushConstants> pushConstants;
    vsg::ref_ptr<vsg::TraceRays> traceRays;

    //shader binding table for trace rays
    vsg::ref_ptr<vsg::RayTracingShaderBindingTable> shaderBindingTable;

    //binding map containing all descriptor bindings in the shaders
    vsg::BindingMap bindingMap;
protected:
    std::vector<bool> opaqueGeometries;

    class ConstantInfosValue : public vsg::Inherit<vsg::Value<ConstantInfos>, ConstantInfosValue>{
        public:
        ConstantInfosValue(){}
    };

    bool useExternalGBuffer;

    //handles setup of the raygen shader.
    //curently does nothing as only one raygen shader exists
    vsg::ref_ptr<vsg::ShaderStage> setupRaygenShader(std::string raygenPath){
        std::vector<std::string> defines; //needed defines for the correct illumination buffer
        auto final = illuminationBuffer.cast<IlluminationBufferFinal>();
        auto finalDirIndir = illuminationBuffer.cast<IlluminationBufferFinalDirIndir>();
        auto finalDemod = illuminationBuffer.cast<IlluminationBufferFinalDemodulated>();
        auto demod = illuminationBuffer.cast<IlluminationBufferDemodulated>();
        //set different raygen shaders according to state of external gbuffer and illumination buffer type
        //TODO: implement things for external gBuffer
        if(useExternalGBuffer){
            if(final){
                //raygenPath = "shaders/raygen.rgen.spv";
            }
            else if(finalDirIndir){
                //raygenPath = "shaders/raygen.rgen.spv";
            }
            else{
                //raygenPath = "shaders/raygen.rgen.spv";
            }
        }
        else{
            if(final){
                defines.push_back("FINAL_IMAGE");
            }
            else if(finalDirIndir){
                //TODO:
            }
            else if(finalDemod){
                defines.push_back("FINAL_IMAGE");
                defines.push_back("DEMOD_ILLUMINATION");
                defines.push_back("DEMOD_ILLUMINATION_SQUARED");
                if(gBuffer)
                    defines.push_back("GBUFFER");
                if(accumulationBuffer)
                    defines.push_back("PREV_GBUFFER");
            }
            else if(demod){
                defines.push_back("DEMOD_ILLUMINATION");
                defines.push_back("DEMOD_ILLUMINATION_SQUARED");
                if(gBuffer)
                    defines.push_back("GBUFFER");
                if(accumulationBuffer)
                    defines.push_back("PREV_GBUFFER");
            }
            else{
                throw vsg::Exception{"Error: PBRTPipeline::setupRaygenShader(...) Illumination buffer not supported."};
            }
        }

        auto options = vsg::Options::create(vsgXchange::glsl::create());
        auto raygenShader = vsg::ShaderStage::read(VK_SHADER_STAGE_RAYGEN_BIT_KHR, "main", raygenPath, options);
        auto shaderCompiler = vsg::ShaderCompiler::create();
        vsg::Paths searchPaths{"shaders"};
        shaderCompiler->compile(raygenShader, defines, searchPaths);

        return raygenShader;
    }

    void setupPipeline(vsg::Node* scene){
        if(!illuminationBuffer.cast<IlluminationBufferFinal>())     //only create accumulation buffer if no accumulation is needed
            accumulationBuffer = AccumulationBuffer::create(width, height);

        //creating the shader stages and shader binding table
        std::string raygenPath = "shaders/raygen.rgen";             //raygen shader not yet precompiled
        std::string raymissPath = "shaders/miss.rmiss.spv";
        std::string shadowMissPath = "shaders/shadow.rmiss.spv";
        std::string closesthitPath = "shaders/pbr_closesthit.rchit.spv";
        std::string anyHitPath = "shaders/alpha_hit.rahit.spv";

        auto raygenShader = setupRaygenShader(raygenPath);
        auto raymissShader = vsg::ShaderStage::readSpv(VK_SHADER_STAGE_MISS_BIT_KHR, "main", raymissPath);
        auto shadowMissShader = vsg::ShaderStage::readSpv(VK_SHADER_STAGE_MISS_BIT_KHR, "main", shadowMissPath);
        auto closesthitShader = vsg::ShaderStage::readSpv(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "main", closesthitPath);
        auto anyHitShader = vsg::ShaderStage::readSpv(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, "main", anyHitPath);
        if(!raygenShader || !raymissShader || !closesthitShader || !shadowMissShader || !anyHitShader){
            throw vsg::Exception{"Error: PBRTPipeline::PBRTPipeline(...) failed to create shader stages."};
        }
        bindingMap = vsg::ShaderStage::mergeBindingMaps(
            {raygenShader->getDescriptorSetLayoutBindingsMap(), 
            raymissShader->getDescriptorSetLayoutBindingsMap(),
            shadowMissShader->getDescriptorSetLayoutBindingsMap(),
            closesthitShader->getDescriptorSetLayoutBindingsMap(),
            anyHitShader->getDescriptorSetLayoutBindingsMap()});

        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(bindingMap.begin()->second.bindings);
        //auto rayTracingPipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{{VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(RayTracingPushConstants)}});
        auto rayTracingPipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, raygenShader->getPushConstantRanges());
        auto shaderStage = vsg::ShaderStages{raygenShader, raymissShader, shadowMissShader, closesthitShader, anyHitShader};
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
        auto transparenthitShaderGroup = vsg::RayTracingShaderGroup::create();
        transparenthitShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        transparenthitShaderGroup->closestHitShader = 3;
        transparenthitShaderGroup->anyHitShader = 4;
        auto shaderGroups = vsg::RayTracingShaderGroups{raygenShaderGroup, raymissShaderGroup, shadowMissShaderGroup, closesthitShaderGroup, transparenthitShaderGroup};
        shaderBindingTable = vsg::RayTracingShaderBindingTable::create();
        shaderBindingTable->bindingTableEntries.raygenGroups = {raygenShaderGroup};
        shaderBindingTable->bindingTableEntries.raymissGroups = {raymissShaderGroup, shadowMissShaderGroup};
        shaderBindingTable->bindingTableEntries.hitGroups = {closesthitShaderGroup, transparenthitShaderGroup};
        auto pipeline = vsg::RayTracingPipeline::create(rayTracingPipelineLayout, shaderStage, shaderGroups, shaderBindingTable, 2 * maxRecursionDepth);
        bindRayTracingPipeline = vsg::BindRayTracingPipeline::create(pipeline);

        //parsing data from scene
        CreateRayTracingDescriptorTraversal buildDescriptorBinding;
        scene->accept(buildDescriptorBinding);
        bindRayTracingDescriptorSet = buildDescriptorBinding.getBindDescriptorSet(rayTracingPipelineLayout, bindingMap);
        opaqueGeometries = buildDescriptorBinding.isOpaque;

        //creating the constant infos uniform buffer object
        auto constantInfos = ConstantInfosValue::create();
        constantInfos->value().lightCount = buildDescriptorBinding.packedLights.size();
        constantInfos->value().maxRecursionDepth = maxRecursionDepth;
        uint32_t uniformBufferBinding = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Infos").second;
        auto constantInfosDescriptor = vsg::DescriptorBuffer::create(constantInfos, uniformBufferBinding, 0);
        bindRayTracingDescriptorSet->descriptorSet->descriptors.push_back(constantInfosDescriptor);

        //update the descriptor sets
        illuminationBuffer->updateDescriptor(bindRayTracingDescriptorSet, bindingMap);
        if(gBuffer)
            gBuffer->updateDescriptor(bindRayTracingDescriptorSet, bindingMap);
        if(accumulationBuffer)
            accumulationBuffer->updateDescriptor(bindRayTracingDescriptorSet, bindingMap);
    }
};
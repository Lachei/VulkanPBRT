#pragma once

#include "Defines.hpp"
#include <vsg/all.h>
#include <vsgXchange/glsl.h>
#include "GBuffer.hpp"
#include "IlluminationBuffer.hpp"
#include "RayTracingVisitor.hpp"
#include "AccumulationBuffer.hpp"

class PBRTPipeline: public vsg::Inherit<vsg::Object, PBRTPipeline>{
public:
    struct ConstantInfos{
        uint lightCount;
        uint minRecursionDepth;
        uint maxRecursionDepth;
    };

    PBRTPipeline(uint width, uint height, uint maxRecursionDepth, vsg::Node* scene, vsg::ref_ptr<IlluminationBuffer> illuminationBuffer): 
    width(width), height(height), maxRecursionDepth(maxRecursionDepth), illuminationBuffer(illuminationBuffer){
        //GBuffer setup
        useExternalGBuffer = false;
        gBuffer = GBuffer::create(width, height);

        setupPipeline(scene);
    }
    PBRTPipeline(uint width, uint height, uint maxRecursionDepth, vsg::Node* scene, vsg::ref_ptr<IlluminationBuffer> illuminationBuffer, vsg::ref_ptr<GBuffer> gBuffer): 
    width(width), height(height), maxRecursionDepth(maxRecursionDepth), illuminationBuffer(illuminationBuffer), gBuffer(gBuffer){
        useExternalGBuffer = true;

        setupPipeline(scene);
    }

    void compile(vsg::Context& context){
        gBuffer->compile(context);
        illuminationBuffer->compile(context);
        accumulationBuffer->compile(context);
    }

    //handles setup of the raygen shader.
    //curently does nothing as only one raygen shader exists
    void setupRaygenShader(){
        auto final = illuminationBuffer.cast<IlluminationBufferFinal>();
        auto finalDirIndir = illuminationBuffer.cast<IlluminationBufferFinalDirIndir>();
        auto finalDemod = illuminationBuffer.cast<IlluminationBufferFinalDemodulated>();
        //set different raygen shaders according to state of external gbuffer and illumination buffer type
        //TODO: create different shaders for correct model
        std::string raygenPath = "shaders/raygen.rgen.spv";
        if(useExternalGBuffer){
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
        //raygenShader->module = vsg::ShaderModule::read(raygenPath);

        //create additional accumulation images dependant on illumination buffer type
        if(final){

        }
        else if(finalDirIndir){

        }
        else if(finalDemod){
                   
        }
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
        gBuffer->updateImageLayouts(context);
        illuminationBuffer->updateImageLayouts(context);
        accumulationBuffer->updateImageLayouts(context);
    }

    void cmdCopyToAccImages(vsg::ref_ptr<vsg::Commands> commands){
        accumulationBuffer->copyToBackImages(commands, gBuffer, illuminationBuffer);
    }

    uint width, height, maxRecursionDepth, samplePerPixel;
    vsg::ref_ptr<GBuffer> gBuffer;
    vsg::ref_ptr<IlluminationBuffer> illuminationBuffer;        //is retrieved in the constructor
    vsg::ref_ptr<AccumulationBuffer> accumulationBuffer;
    vsg::ref_ptr<vsg::ShaderStage> raygenShader;

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

    void setupPipeline(vsg::Node* scene){
        accumulationBuffer = AccumulationBuffer::create(width, height);

        setupRaygenShader();    //currently doesnt do anything, only main raygenshader exists

        //creating the shader stages and shader binding table
        std::string raygenPath = "shaders/raygen.rgen.spv";     //default reaygen shader
        std::string raymissPath = "shaders/miss.rmiss.spv";
        std::string shadowMissPath = "shaders/shadow.rmiss.spv";
        std::string closesthitPath = "shaders/pbr_closesthit.rchit.spv";
        std::string anyHitPath = "shaders/alpha_hit.rahit.spv";

        auto options = vsg::Options::create(vsgXchange::glsl::create());
        raygenShader = vsg::ShaderStage::readSpv(VK_SHADER_STAGE_RAYGEN_BIT_KHR, "main", raygenPath);
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
        uint uniformBufferBinding = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Infos").second;
        auto constantInfosDescriptor = vsg::DescriptorBuffer::create(constantInfos, uniformBufferBinding, 0);
        bindRayTracingDescriptorSet->descriptorSet->descriptors.push_back(constantInfosDescriptor);

        //update the descriptor sets
        gBuffer->updateDescriptor(bindRayTracingDescriptorSet, bindingMap);
        illuminationBuffer->updateDescriptor(bindRayTracingDescriptorSet, bindingMap);
        accumulationBuffer->updateDescriptor(bindRayTracingDescriptorSet, bindingMap);
    }
};
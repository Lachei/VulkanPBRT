#include "PBRTPipeline.hpp"

#include <cassert>

namespace
{
    struct ConstantInfos
    {
        uint32_t lightCount;
        uint32_t minRecursionDepth;
        uint32_t maxRecursionDepth;
    };

    class ConstantInfosValue : public vsg::Inherit<vsg::Value<ConstantInfos>, ConstantInfosValue>
    {
    public:
        ConstantInfosValue()
        {
        }
    };
}

PBRTPipeline::PBRTPipeline(vsg::ref_ptr<vsg::Node> scene, vsg::ref_ptr<GBuffer> gBuffer, vsg::ref_ptr<AccumulationBuffer> accumulationBuffer,
                 vsg::ref_ptr<IlluminationBuffer> illuminationBuffer, bool writeGBuffer, RayTracingRayOrigin rayTracingRayOrigin) : 
    width(illuminationBuffer->illuminationImages[0]->imageInfoList[0].imageView->image->extent.width), 
    height(illuminationBuffer->illuminationImages[0]->imageInfoList[0].imageView->image->extent.height), 
    maxRecursionDepth(2), 
    accumulationBuffer(accumulationBuffer),
    illuminationBuffer(illuminationBuffer),
    gBuffer(gBuffer)
{
    if (writeGBuffer) assert(gBuffer);
    bool useExternalGBuffer = rayTracingRayOrigin == RayTracingRayOrigin::GBUFFER;
    setupPipeline(scene, useExternalGBuffer);
}
void PBRTPipeline::setTlas(vsg::ref_ptr<vsg::AccelerationStructure> as)
{
    auto tlas = as.cast<vsg::TopLevelAccelerationStructure>();
    assert(tlas);
    for (int i = 0; i < tlas->geometryInstances.size(); ++i)
    {
        if (opaqueGeometries[i])
            tlas->geometryInstances[i]->shaderOffset = 0;
        else
            tlas->geometryInstances[i]->shaderOffset = 1;
        tlas->geometryInstances[i]->geometryFlags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
    }
    auto accelDescriptor = vsg::DescriptorAccelerationStructure::create(vsg::AccelerationStructures{as}, 0, 0);
    bindRayTracingDescriptorSet->descriptorSet->descriptors.push_back(accelDescriptor);
}
void PBRTPipeline::compile(vsg::Context &context)
{
    illuminationBuffer->compile(context);
}
void PBRTPipeline::updateImageLayouts(vsg::Context &context)
{
    illuminationBuffer->updateImageLayouts(context);
}
void PBRTPipeline::addTraceRaysToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph, vsg::ref_ptr<vsg::PushConstants> pushConstants)
{
    commandGraph->addChild(bindRayTracingPipeline);
    commandGraph->addChild(bindRayTracingDescriptorSet);
    commandGraph->addChild(pushConstants);
    auto traceRays = vsg::TraceRays::create();
    traceRays->bindingTable = shaderBindingTable;
    traceRays->width = width;
    traceRays->height = height;
    traceRays->depth = 1;
    commandGraph->addChild(traceRays);
}
vsg::ref_ptr<IlluminationBuffer> PBRTPipeline::getIlluminationBuffer() const
{
    return illuminationBuffer;
}
void PBRTPipeline::setupPipeline(vsg::Node *scene, bool useExternalGbuffer)
{
    //creating the shader stages and shader binding table
    std::string raygenPath = "shaders/ptRaygen.rgen"; //raygen shader not yet precompiled
    std::string raymissPath = "shaders/ptMiss.rmiss.spv";
    std::string shadowMissPath = "shaders/shadow.rmiss.spv";
    std::string closesthitPath = "shaders/ptClosesthit.rchit.spv";
    std::string anyHitPath = "shaders/ptAlphaHit.rahit.spv";

    auto raygenShader = setupRaygenShader(raygenPath, useExternalGbuffer);
    auto raymissShader = vsg::ShaderStage::readSpv(VK_SHADER_STAGE_MISS_BIT_KHR, "main", raymissPath);
    auto shadowMissShader = vsg::ShaderStage::readSpv(VK_SHADER_STAGE_MISS_BIT_KHR, "main", shadowMissPath);
    auto closesthitShader = vsg::ShaderStage::readSpv(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "main", closesthitPath);
    auto anyHitShader = vsg::ShaderStage::readSpv(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, "main", anyHitPath);
    if (!raygenShader || !raymissShader || !closesthitShader || !shadowMissShader || !anyHitShader)
    {
        throw vsg::Exception{"Error: PBRTPipeline::PBRTPipeline(...) failed to create shader stages."};
    }
    bindingMap = vsg::ShaderStage::mergeBindingMaps(
        {raygenShader->getDescriptorSetLayoutBindingsMap(),
         raymissShader->getDescriptorSetLayoutBindingsMap(),
         shadowMissShader->getDescriptorSetLayoutBindingsMap(),
         closesthitShader->getDescriptorSetLayoutBindingsMap(),
         anyHitShader->getDescriptorSetLayoutBindingsMap()});

    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(bindingMap.begin()->second.bindings);
    // auto rayTracingPipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{{VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(RayTracingPushConstants)}});
    auto rayTracingPipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout},
                                                                raygenShader->getPushConstantRanges());
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
    auto shaderGroups = vsg::RayTracingShaderGroups{
        raygenShaderGroup, raymissShaderGroup, shadowMissShaderGroup, closesthitShaderGroup, transparenthitShaderGroup};
    shaderBindingTable = vsg::RayTracingShaderBindingTable::create();
    shaderBindingTable->bindingTableEntries.raygenGroups = {raygenShaderGroup};
    shaderBindingTable->bindingTableEntries.raymissGroups = {raymissShaderGroup, shadowMissShaderGroup};
    shaderBindingTable->bindingTableEntries.hitGroups = {closesthitShaderGroup, transparenthitShaderGroup};
    auto pipeline = vsg::RayTracingPipeline::create(rayTracingPipelineLayout, shaderStage, shaderGroups, shaderBindingTable);
    bindRayTracingPipeline = vsg::BindRayTracingPipeline::create(pipeline);

    // parsing data from scene
    RayTracingSceneDescriptorCreationVisitor buildDescriptorBinding;
    scene->accept(buildDescriptorBinding);
    bindRayTracingDescriptorSet = buildDescriptorBinding.getBindDescriptorSet(rayTracingPipelineLayout, bindingMap);
    opaqueGeometries = buildDescriptorBinding.isOpaque;

    // creating the constant infos uniform buffer object
    auto constantInfos = ConstantInfosValue::create();
    constantInfos->value().lightCount = buildDescriptorBinding.packedLights.size();
    constantInfos->value().maxRecursionDepth = maxRecursionDepth;
    uint32_t uniformBufferBinding = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Infos").second;
    auto constantInfosDescriptor = vsg::DescriptorBuffer::create(constantInfos, uniformBufferBinding, 0);
    bindRayTracingDescriptorSet->descriptorSet->descriptors.push_back(constantInfosDescriptor);

    // update the descriptor sets
    illuminationBuffer->updateDescriptor(bindRayTracingDescriptorSet, bindingMap);
    if (gBuffer)
        gBuffer->updateDescriptor(bindRayTracingDescriptorSet, bindingMap);
    if (accumulationBuffer)
        accumulationBuffer->updateDescriptor(bindRayTracingDescriptorSet, bindingMap);
}
vsg::ref_ptr<vsg::ShaderStage> PBRTPipeline::setupRaygenShader(std::string raygenPath, bool useExternalGBuffer)
{
    std::vector<std::string> defines; // needed defines for the correct illumination buffer
    
    // set different raygen shaders according to state of external gbuffer and illumination buffer type
    if (useExternalGBuffer)
    {
        // TODO: implement things for external gBuffer
        // raygenPath = "shaders/raygen.rgen.spv";
    }
    else
    {
        if (illuminationBuffer.cast<IlluminationBufferFinal>())
        {
            defines.push_back("FINAL_IMAGE");
        }
        else if (illuminationBuffer.cast<IlluminationBufferFinalDirIndir>())
        {
            // TODO:
        }
        else if (illuminationBuffer.cast<IlluminationBufferFinalDemodulated>())
        {
            defines.push_back("FINAL_IMAGE");
            defines.push_back("DEMOD_ILLUMINATION");
            defines.push_back("DEMOD_ILLUMINATION_SQUARED");
            if (gBuffer)
                defines.push_back("GBUFFER");
            if (accumulationBuffer)
                defines.push_back("PREV_GBUFFER");
        }
        else if (illuminationBuffer.cast<IlluminationBufferDemodulated>())
        {
            defines.push_back("DEMOD_ILLUMINATION");
            defines.push_back("DEMOD_ILLUMINATION_SQUARED");
            if (gBuffer)
                defines.push_back("GBUFFER");
            if (accumulationBuffer)
                defines.push_back("PREV_GBUFFER");
        }
        else
        {
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

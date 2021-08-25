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
        buffer->updateDescriptor(bindRayTracingDescriptorSet, bindingMap.begin()->second.names);
        illuminationBuffer = buffer;

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
        raygenShader->module = vsg::ShaderModule::read(raygenPath);

        //create additional accumulation images dependant on illumination buffer type
        if(final){

        }
        else if(finalDirIndir){

        }
        else if(finalDemod){
            std::vector<std::string>& bindingNames = bindingMap.begin()->second.names;
            auto image = vsg::Image::create();
            image->imageType = VK_IMAGE_TYPE_2D;
            image->format = VK_FORMAT_R16G16B16A16_SFLOAT;
            image->extent.width = width;
            image->extent.height = height;
            image->extent.depth = 1;
            image->mipLevels = 1;
            image->arrayLayers = 1;
            image->samples = VK_SAMPLE_COUNT_1_BIT;
            image->tiling = VK_IMAGE_TILING_OPTIMAL;
            image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            auto imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
            vsg::ImageInfo imageInfo{sampler, imageView, VK_IMAGE_LAYOUT_GENERAL};
            int demodAccIndex = std::find(bindingNames.begin(), bindingNames.end(), "prevOutput") - bindingNames.begin();
            demodAcc = vsg::DescriptorImage::create(imageInfo, demodAccIndex, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            bindRayTracingDescriptorSet->descriptorSet->descriptors.push_back(demodAcc);

            image = vsg::Image::create();
            image->imageType = VK_IMAGE_TYPE_2D;
            image->format = VK_FORMAT_R16G16B16A16_SFLOAT;
            image->extent.width = width;
            image->extent.height = height;
            image->extent.depth = 1;
            image->mipLevels = 1;
            image->arrayLayers = 1;
            image->samples = VK_SAMPLE_COUNT_1_BIT;
            image->tiling = VK_IMAGE_TILING_OPTIMAL;
            image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
            imageInfo = {sampler, imageView, VK_IMAGE_LAYOUT_GENERAL};
            int demodAccIndex = std::find(bindingNames.begin(), bindingNames.end(), "prevIlluminationSquared") - bindingNames.begin();
            demodAccSquared = vsg::DescriptorImage::create(imageInfo, demodAccIndex, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            bindRayTracingDescriptorSet->descriptorSet->descriptors.push_back(demodAccSquared);        
        }
    }

    void setTlas(vsg::ref_ptr<vsg::AccelerationStructure> as){
        auto accelDescriptor = vsg::DescriptorAccelerationStructure::create(vsg::AccelerationStructures{as}, 0, 0);
        bindRayTracingDescriptorSet->descriptorSet->descriptors.push_back(accelDescriptor);
    }

    void compileImages(vsg::Context& context){
        sampleAcc->compile(context);
        if(demodAcc)
            demodAcc->compile(context);
        if(demodAccSquared)
            demodAccSquared->compile(context);
    }

    void updateImageLayouts(vsg::Context& context){
        auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT);
        VkImageSubresourceRange resourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0 , 1, 0, 1};
        auto sampleAccLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, sampleAcc->imageInfoList[0].imageView->image, resourceRange);
        pipelineBarrier->imageMemoryBarriers.push_back(sampleAccLayout);
        if(demodAcc){
            auto demodAccLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, demodAcc->imageInfoList[0].imageView->image, resourceRange);
            pipelineBarrier->imageMemoryBarriers.push_back(demodAccLayout);
        }
        if(demodAccSquared){
            auto demodAccLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, demodAccSquared->imageInfoList[0].imageView->image, resourceRange);
            pipelineBarrier->imageMemoryBarriers.push_back(demodAccLayout);
        }
        context.commands.push_back(pipelineBarrier);
    }

    void copyToAccImages(vsg::ref_ptr<vsg::Commands> commands){
        auto srcImage = gBuffer->sample->imageInfoList[0].imageView->image;
        auto dstImage = sampleAcc->imageInfoList[0].imageView->image;

        VkImageSubresourceRange resourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0 , 1, 0, 1};
        auto srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0, srcImage, resourceRange);
        auto dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, dstImage, resourceRange);
        auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                            srcBarrier, dstBarrier);
        commands->addChild(pipelineBarrier);

        VkImageCopy copyRegion{};
        copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.srcOffset = {0, 0, 0};
        copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.dstOffset = {0, 0, 0};
        copyRegion.extent = {width, height, 1};

        auto copyImage = vsg::CopyImage::create();
        copyImage->srcImage = srcImage;
        copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        copyImage->dstImage = dstImage;
        copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copyImage->regions.emplace_back(copyRegion);
        commands->addChild(copyImage);

        srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, srcImage, resourceRange);
        dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, dstImage, resourceRange);
        pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                            srcBarrier, dstBarrier);
        commands->addChild(pipelineBarrier);

        if(demodAcc){
            srcImage = illuminationBuffer->illuminationImages[1]->imageInfoList[0].imageView->image;
            dstImage = demodAcc->imageInfoList[0].imageView->image;

            srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0, srcImage, resourceRange);
            dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, dstImage, resourceRange);
            pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                srcBarrier, dstBarrier);
            commands->addChild(pipelineBarrier);

            copyImage = vsg::CopyImage::create();
            copyImage->srcImage = srcImage;
            copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            copyImage->dstImage = dstImage;
            copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            copyImage->regions.emplace_back(copyRegion);
            commands->addChild(copyImage);

            srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, srcImage, resourceRange);
            dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, dstImage, resourceRange);
            pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                srcBarrier, dstBarrier);
            commands->addChild(pipelineBarrier);
        }
        if(demodAccSquared){
            srcImage = illuminationBuffer->illuminationImages[1]->imageInfoList[0].imageView->image;
            dstImage = demodAccSquared->imageInfoList[0].imageView->image;

            srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0, srcImage, resourceRange);
            dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0, dstImage, resourceRange);
            pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                srcBarrier, dstBarrier);
            commands->addChild(pipelineBarrier);

            copyImage = vsg::CopyImage::create();
            copyImage->srcImage = srcImage;
            copyImage->srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            copyImage->dstImage = dstImage;
            copyImage->dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            copyImage->regions.emplace_back(copyRegion);
            commands->addChild(copyImage);

            srcBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, srcImage, resourceRange);
            dstBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0, dstImage, resourceRange);
            pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                srcBarrier, dstBarrier);
            commands->addChild(pipelineBarrier);
        }
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

    //optional resources for data accumulation
    vsg::ref_ptr<vsg::DescriptorImage> demodAcc, demodAccSquared, sampleAcc;
    uint demodAccBinding = 23, demodAccSquaredBinding = 28, sampleAccBinding = 24, uniformBufferBinding = 26;

    //shader binding table for trace rays
    vsg::ref_ptr<vsg::RayTracingShaderBindingTable> shaderBindingTable;

    //general bilinear sampler
    vsg::ref_ptr<vsg::Sampler> sampler;

    //binding map containing all descriptor bindings in the shaders
    vsg::BindingMap bindingMap;
protected:
    class ConstantInfosValue : public vsg::Inherit<vsg::Value<ConstantInfos>, ConstantInfosValue>{
        public:
        ConstantInfosValue(){}
    };

    bool useExternalGBuffer;

    void setupPipeline(vsg::Node* scene){
        //creating standard bilinear sampler
        sampler = vsg::Sampler::create();

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
        bindingMap = vsg::mergeBindingMaps(
            {raygenShader->getDescriptorSetLayoutBindingsMap(), 
            raymissShader->getDescriptorSetLayoutBindingsMap(),
            shadowMissShader->getDescriptorSetLayoutBindingsMap(),
            closesthitShader->getDescriptorSetLayoutBindingsMap()});

        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(bindingMap.begin()->second.bindings);
        auto rayTracingPipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout});
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

        //parsing data from scene
        CreateRayTracingDescriptorTraversal buildDescriptorBinding;
        scene->accept(buildDescriptorBinding);
        bindRayTracingDescriptorSet = buildDescriptorBinding.getBindDescriptorSet(rayTracingPipelineLayout, bindingMap.begin()->second.names);

        //creating the constant infos uniform buffer object
        auto constantInfos = ConstantInfosValue::create();
        constantInfos->value().lightCount = buildDescriptorBinding.packedLights.size();
        constantInfos->value().maxRecursionDepth = maxRecursionDepth;
        constantInfos->value().minRecursionDepth = maxRecursionDepth;
        auto constantInfosDescriptor = vsg::DescriptorBuffer::create(constantInfos, uniformBufferBinding, 0);
        bindRayTracingDescriptorSet->descriptorSet->descriptors.push_back(constantInfosDescriptor);
        rayTracingPipelineLayout->setLayouts[0]->bindings.push_back({uniformBufferBinding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr});


        //adding gbuffer bindings to the descriptor
        gBuffer->updateDescriptor(bindRayTracingDescriptorSet, bindingMap.begin()->second.names);

        //creating sample accumulation image
        std::vector<std::string>& bindingNames = bindingMap.begin()->second.names;
        auto image = vsg::Image::create();
        image->imageType = VK_IMAGE_TYPE_2D;
        image->format = VK_FORMAT_R8_UNORM;
        image->extent.width = width;
        image->extent.height = height;
        image->extent.depth = 1;
        image->mipLevels = 1;
        image->arrayLayers = 1;
        image->samples = VK_SAMPLE_COUNT_1_BIT;
        image->tiling = VK_IMAGE_TILING_OPTIMAL;
        image->usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        auto imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
        vsg::ImageInfo imageInfo{sampler, imageView, VK_IMAGE_LAYOUT_GENERAL};
        int sampleAccIndex = std::find(bindingNames.begin(), bindingNames.end(), "prevSampleCounts") - bindingNames.begin();
        sampleAcc = vsg::DescriptorImage::create(imageInfo, sampleAccIndex, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        bindRayTracingDescriptorSet->descriptorSet->descriptors.push_back(sampleAcc);
    }
};
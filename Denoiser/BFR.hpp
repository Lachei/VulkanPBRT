#pragma once
#include <vsg/all.h>
#include <string>
#include "../GBuffer.hpp"
#include "../IlluminationBuffer.hpp"
#include "../TAA/taa.hpp"
#include "../PipelineStructs.hpp"

class BFR: public vsg::Inherit<vsg::Object, BFR>{
public:
    uint depthBinding = 0, normalBinding = 1, materialBinding = 2, albedoBinding = 3, prevDepthBinding = -1, prevNormalBinding = -1, motionBinding = 4, sampleBinding = 5, sampledDenIlluBinding = 6, denoisedIlluBinding = 9, finalBinding = 7, noisyBinding = 8, denoisedBinding = 9;
    
    BFR(uint width, uint height, uint workWidth, uint workHeight, vsg::ref_ptr<GBuffer> gBuffer, vsg::ref_ptr<IlluminationBuffer> illuBuffer, vsg::ref_ptr<AccumulationBuffer> accBuffer):
    width(width),
    height(height),
    workWidth(workWidth),
    workHeight(workHeight),
    gBuffer(gBuffer),
    sampler(vsg::Sampler::create())
    {
        if(!illuBuffer.cast<IlluminationBufferFinalDemodulated>()) return;        //demodulated illumination needed 
        auto illumination = illuBuffer;
        std::string shaderPath = "shaders/bfr.comp.spv";
        auto computeStage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", shaderPath);
        computeStage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
            {0, vsg::intValue::create(width)},
            {1, vsg::intValue::create(height)},
            {2, vsg::intValue::create(workWidth)},
            {3, vsg::intValue::create(workHeight)}};

        // denoised illuminatino accumulation
        auto image = vsg::Image::create();
        image->imageType = VK_IMAGE_TYPE_2D;
        image->format = VK_FORMAT_R16G16B16A16_SFLOAT;
        image->extent.width = width;
        image->extent.height = height;
        image->extent.depth = 1;
        image->mipLevels = 1;
        image->arrayLayers = 2;
        image->samples = VK_SAMPLE_COUNT_1_BIT;
        image->tiling = VK_IMAGE_TILING_OPTIMAL;
        image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        auto imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
        imageView->viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        vsg::ImageInfo imageInfo = {nullptr, imageView, VK_IMAGE_LAYOUT_GENERAL};
        accumulatedIllumination = vsg::DescriptorImage::create(imageInfo, denoisedIlluBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        imageInfo.sampler = sampler;
        sampledAccIllu = vsg::DescriptorImage::create(imageInfo, sampledDenIlluBinding, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

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
        image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
        imageInfo = {nullptr, imageView, VK_IMAGE_LAYOUT_GENERAL};
        finalIllumination = vsg::DescriptorImage::create(imageInfo, finalBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        // descriptor set layout
        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {5, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {7, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {8, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            {9, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}
        };
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        // filling descriptor set
        vsg::Descriptors descriptors{
            vsg::DescriptorImage::create(gBuffer->depth->imageInfoList[0], depthBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
            vsg::DescriptorImage::create(gBuffer->normal->imageInfoList[0], normalBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
            vsg::DescriptorImage::create(gBuffer->material->imageInfoList[0], materialBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
            vsg::DescriptorImage::create(gBuffer->albedo->imageInfoList[0], albedoBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
            vsg::DescriptorImage::create(accBuffer->motion->imageInfoList[0], motionBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
            vsg::DescriptorImage::create(accBuffer->spp->imageInfoList[0], sampleBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
            vsg::DescriptorImage::create(illumination->illuminationImages[1]->imageInfoList[0], noisyBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
            accumulatedIllumination,
            finalIllumination,
            sampledAccIllu
        };
        auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, descriptors);

        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(RayTracingPushConstants)}});
        bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);

        bfrPipeline = vsg::ComputePipeline::create(pipelineLayout, computeStage);
        bindBfrPipeline = vsg::BindComputePipeline::create(bfrPipeline);

        taaPipeline = Taa::create(width, height, workWidth, workHeight, gBuffer, accBuffer, finalIllumination);
    }

    void addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph, vsg::ref_ptr<vsg::PushConstants> pushConstants){
        commandGraph->addChild(bindBfrPipeline);
        commandGraph->addChild(bindDescriptorSet);
        commandGraph->addChild(pushConstants);
        commandGraph->addChild(vsg::Dispatch::create((width / workWidth + 1), (height / workHeight + 1), 1));
        auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT);
        commandGraph->addChild(pipelineBarrier);        //barrier to wait for completion of denoising before taa is applied
        taaPipeline->addDispatchToCommandGraph(commandGraph);
    }

    void compile(vsg::Context& context){
        accumulatedIllumination->compile(context);
        sampledAccIllu->compile(context);
        finalIllumination->compile(context);
        taaPipeline->compile(context);
    }

    void updateImageLayouts(vsg::Context& context){
        VkImageSubresourceRange resourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0 , 1, 0, 2};
        auto accIlluLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, accumulatedIllumination->imageInfoList[0].imageView->image, resourceRange);
        resourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0 , 1, 0, 1};
        auto finalIluLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, finalIllumination->imageInfoList[0].imageView->image, resourceRange);

        auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT,
            accIlluLayout, finalIluLayout);
        context.commands.push_back(pipelineBarrier);
        taaPipeline->updateImageLayouts(context);
    }

    vsg::ref_ptr<vsg::ComputePipeline> bfrPipeline;
    vsg::ref_ptr<vsg::BindComputePipeline> bindBfrPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;

    vsg::ref_ptr<vsg::DescriptorImage> accumulatedIllumination, sampledAccIllu, finalIllumination;
    vsg::ref_ptr<Taa> taaPipeline;

    vsg::ref_ptr<vsg::Sampler> sampler;
protected:
    uint width, height, workWidth, workHeight;
    vsg::ref_ptr<GBuffer> gBuffer;
};
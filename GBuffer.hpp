#pragma once

#include <vsg/all.h>

// class holding all references for the gbuffer
// supports automatically updating the ray tracing descriptor set
// care to update also the RayTracingVisitor.hpp descriptor layout if you add things to the gbuffer
class GBuffer: public vsg::Inherit<vsg::Object, GBuffer>{
public:
    GBuffer(uint width, uint height): width(width), height(height){setupImages();}

    uint width, height;
    vsg::ref_ptr<vsg::DescriptorImage> depth, normal, material, albedo, prevDepth, prevNormal, motion, sample, prevSample;

    void updateDescriptor(vsg::BindDescriptorSet* descSet, const vsg::BindingMap& bindingMap){
        vsg::DescriptorSetLayoutBindings& bindings = descSet->descriptorSet->setLayout->bindings;
        int depthInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "depthImage").second;
        depth->dstBinding = depthInd;
        int normalInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "normalImage").second;
        normal->dstBinding = normalInd;
        int materialInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "materialImage").second;
        material->dstBinding = materialInd;
        int albedoInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "albedoImage").second;
        albedo->dstBinding = albedoInd;
        int prevDepthInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "prevDepth").second;
        prevDepth->dstBinding = prevDepthInd;
        int prevNormalInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "prevNormal").second;
        prevNormal->dstBinding = prevNormalInd;
        int motionInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "motion").second;
        motion->dstBinding = motionInd;
        int sampleInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "sampleCounts").second;
        sample->dstBinding = sampleInd;
        
        descSet->descriptorSet->descriptors.push_back(depth);
        descSet->descriptorSet->descriptors.push_back(normal);
        descSet->descriptorSet->descriptors.push_back(material);
        descSet->descriptorSet->descriptors.push_back(albedo);
        descSet->descriptorSet->descriptors.push_back(prevDepth);
        descSet->descriptorSet->descriptors.push_back(prevNormal);
        descSet->descriptorSet->descriptors.push_back(motion);
        descSet->descriptorSet->descriptors.push_back(sample);
    }

    void updateImageLayouts(vsg::Context& context){
        VkImageSubresourceRange resourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0 , 1, 0, 1};
        auto depthLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, depth->imageInfoList[0].imageView->image, resourceRange);
        auto normalLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, normal->imageInfoList[0].imageView->image, resourceRange);
        auto materialLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, material->imageInfoList[0].imageView->image, resourceRange);
        auto albedoLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, albedo->imageInfoList[0].imageView->image, resourceRange);
        auto prevDepthLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, prevDepth->imageInfoList[0].imageView->image, resourceRange);
        auto prevNormalLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, prevNormal->imageInfoList[0].imageView->image, resourceRange);
        auto motionLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, motion->imageInfoList[0].imageView->image, resourceRange);
        auto sampleLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, sample->imageInfoList[0].imageView->image, resourceRange);
        
        auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT, 
                        depthLayout, normalLayout, materialLayout, albedoLayout, prevDepthLayout, prevNormalLayout, motionLayout, sampleLayout);
        context.commands.push_back(pipelineBarrier);
    }

    void compile(vsg::Context& context){
        depth->compile(context);
        normal->compile(context);
        material->compile(context);
        albedo->compile(context);
        prevDepth->compile(context);
        prevNormal->compile(context);
        motion->compile(context);
        sample->compile(context);
    }

    void copySampleImage(vsg::ref_ptr<vsg::Commands> commands, vsg::ref_ptr<vsg::Image> dstImage){
        auto srcImage = sample->imageInfoList[0].imageView->image;

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
    }

    void copyToPrevImages(vsg::ref_ptr<vsg::Commands> commands){
        auto srcImage = depth->imageInfoList[0].imageView->image;
        auto dstImage = prevDepth->imageInfoList[0].imageView->image;

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

        srcImage = normal->imageInfoList[0].imageView->image;
        dstImage = prevNormal->imageInfoList[0].imageView->image;

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

protected:
    vsg::ref_ptr<vsg::Sampler> sampler;
    void setupImages(){
        sampler = vsg::Sampler::create();

        //all images are bound initially to set 0 binding 0. Correct binding number is set in updateDescriptor()
        auto image = vsg::Image::create();
        image->imageType = VK_IMAGE_TYPE_2D;
        image->format = VK_FORMAT_R32_SFLOAT;
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
        auto imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
        vsg::ImageInfo imageInfo{nullptr, imageView, VK_IMAGE_LAYOUT_GENERAL};
        depth = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        image = vsg::Image::create();
        image->imageType = VK_IMAGE_TYPE_2D;
        image->format = VK_FORMAT_R32G32_SFLOAT;
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
        normal = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        image = vsg::Image::create();
        image->imageType = VK_IMAGE_TYPE_2D;
        image->format = VK_FORMAT_R32_SFLOAT;
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
        material = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        image = vsg::Image::create();
        image->imageType = VK_IMAGE_TYPE_2D;
        image->format = VK_FORMAT_R8G8B8A8_UNORM;
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
        albedo = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        image = vsg::Image::create();
        image->imageType = VK_IMAGE_TYPE_2D;
        image->format = VK_FORMAT_R32_SFLOAT;
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
        prevDepth = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        image = vsg::Image::create();
        image->imageType = VK_IMAGE_TYPE_2D;
        image->format = VK_FORMAT_R32G32_SFLOAT;
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
        prevNormal = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        image = vsg::Image::create();
        image->imageType = VK_IMAGE_TYPE_2D;
        image->format = VK_FORMAT_R16G16_SFLOAT;
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
        motion = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        image = vsg::Image::create();
        image->imageType = VK_IMAGE_TYPE_2D;
        image->format = VK_FORMAT_R8_UNORM;
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
        sample = vsg::DescriptorImage::create(imageInfo, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    }
};
#pragma once
#include <vsg/all.h>

class Taa: public vsg::Inherit<vsg::Object, Taa>{
public:
    uint denoisedBinding = 0, motionBinding = 1, finalImageBinding = 2, accumulationBinding = 3;

    Taa(uint width, uint height, uint workWidth, uint workHeight, vsg::ref_ptr<GBuffer> gBuffer, vsg::ref_ptr<vsg::DescriptorImage> denoised):
    width(width),
    height(height),
    workWidth(workWidth),
    workHeight(workHeight)
    {
        std::string shaderPath = "shader/taa.comp.spv";
        auto computeStage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", shaderPath);
        computeStage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
            {0, vsg::intValue::create(width)},
            {1, vsg::intValue::create(height)},
            {2, vsg::intValue::create(workWidth)},
            {3, vsg::intValue::create(workHeight)}};

        //final image and accumulation creation
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
        image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        auto imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
        vsg::ImageInfo imageInfo = {nullptr, imageView, VK_IMAGE_LAYOUT_GENERAL};
        accumulationImage = vsg::DescriptorImage::create(imageInfo, accumulationBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

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
        finalImage = vsg::DescriptorImage::create(imageInfo, finalImageBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        //descriptor set layout
        vsg::DescriptorSetLayoutBindings descriptorBindings{{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
        //filling descriptor set
        //vsg::Descriptors descriptors{vsg::DescriptorBuffer::create(vsg::BufferInfoList{vsg::BufferInfo(buffer, 0, bufferSize)}, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)};
        auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{
            vsg::DescriptorImage::create(gBuffer->motion->imageInfoList[0], motionBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
            vsg::DescriptorImage::create(denoised->imageInfoList[0], denoisedBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
            finalImage,
            accumulationImage
        });

        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(RayTracingPushConstants)}});
        bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, descriptorSet);

        pipeline = vsg::ComputePipeline::create(pipelineLayout, computeStage);
        bindPipeline = vsg::BindComputePipeline::create(pipeline);
    }

    void addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph){
        commandGraph->addChild(bindPipeline);
        commandGraph->addChild(bindDescriptorSet);
        commandGraph->addChild(vsg::Dispatch::create(uint(ceil(float(width) / float(workWidth))), uint(ceil(float(height) / float(workHeight))), 1));
    }

    void compile(vsg::Context& context){
        finalImage->compile(context);
        accumulationImage->compile(context);
    }

    void updateImageLayout(vsg::Context& context){
        VkImageSubresourceRange resourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0 , 1, 0, 1};
        auto accumulationLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, accumulationImage->imageInfoList[0].imageView->image, resourceRange);
        auto finalLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, finalImage->imageInfoList[0].imageView->image, resourceRange);
        auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                accumulationLayout, finalLayout);
        context.commands.push_back(pipelineBarrier);
    }

    vsg::ref_ptr<vsg::ComputePipeline> pipeline;
    vsg::ref_ptr<vsg::BindComputePipeline> bindPipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;
    vsg::ref_ptr<vsg::DescriptorImage> finalImage, accumulationImage;
protected:
    uint width, height, workWidth, workHeight;
};
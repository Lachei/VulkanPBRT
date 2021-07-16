#pragma once

#include <vsg/all.h>
#include <assert.h>

//base illumination buffer class
class IlluminationBuffer: public vsg::Inherit<vsg::Object, IlluminationBuffer>{
public:
    std::vector<uint> illuminationBindings;
    std::vector<vsg::ref_ptr<vsg::DescriptorImage>> illuminationImages;
    uint width, height;

    void compile(vsg::Context& context){
        for(auto& image: illuminationImages) image->compile(context);
    }

    void updateDescriptor(vsg::BindDescriptorSet* descSet){
        for(int i = 0; i < illuminationBindings.size(); ++i){
            descSet->descriptorSet->descriptors.push_back(illuminationImages[i]);
            descSet->descriptorSet->setLayout->bindings.push_back(VkDescriptorSetLayoutBinding{illuminationBindings[i], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr});
        }
    }

    void updateImageLayouts(vsg::Context& context){
        VkImageSubresourceRange resourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0 , 1, 0, 1};
        auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT);
        for(auto& image: illuminationImages){
            auto imageBarrier = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0, image->imageInfoList[0].imageView->image, resourceRange);
            pipelineBarrier->imageMemoryBarriers.push_back(imageBarrier);
        }
        context.commands.push_back(pipelineBarrier);
    }

    void copyImage(vsg::ref_ptr<vsg::Commands> commands, uint imageIndex, vsg::ref_ptr<vsg::Image> dstImage){
        assert(imageIndex < illuminationImages.size());
        auto srcImage = illuminationImages[imageIndex]->imageInfoList[0].imageView->image;

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
};

class IlluminationBufferFinal: public vsg::Inherit<IlluminationBuffer, IlluminationBufferFinal>{
public:
    IlluminationBufferFinal(uint width, uint height){
        this->width = width;
        this->height = height;
        illuminationBindings.push_back(1);
        fillImages();
    };

    void fillImages(){
        auto image = vsg::Image::create();
        image->imageType = VK_IMAGE_TYPE_2D;
        image->format = VK_FORMAT_B8G8R8A8_UNORM;
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
        illuminationImages.push_back(vsg::DescriptorImage::create(imageInfo, illuminationBindings[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));
    }
};

class IlluminationBufferFinalDirIndir: public vsg::Inherit<IlluminationBuffer, IlluminationBufferFinalDirIndir>{
public:
    IlluminationBufferFinalDirIndir(uint width, uint height){
        this->width = width;
        this->height = height;
        illuminationBindings.push_back(0);  //TODO change to appropriate value
        illuminationBindings.push_back(1);
        illuminationBindings.push_back(2);
        fillImages();
    };

    void fillImages(){
        auto image = vsg::Image::create();
        image->imageType = VK_IMAGE_TYPE_2D;
        image->format = VK_FORMAT_B8G8R8A8_UNORM;
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
        illuminationImages.push_back(vsg::DescriptorImage::create(imageInfo, illuminationBindings[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));
    
        image = vsg::Image::create();
        image->imageType = VK_IMAGE_TYPE_2D;
        image->format = VK_FORMAT_R32G32B32A32_SFLOAT;
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
        illuminationImages.push_back(vsg::DescriptorImage::create(imageInfo, illuminationBindings[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));

        image = vsg::Image::create();
        image->imageType = VK_IMAGE_TYPE_2D;
        image->format = VK_FORMAT_R32G32B32A32_SFLOAT;
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
        illuminationImages.push_back(vsg::DescriptorImage::create(imageInfo, illuminationBindings[2], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));
    }
};

class IlluminationBufferFinalDemodulated: public vsg::Inherit<IlluminationBuffer, IlluminationBufferFinalDemodulated>{
public:
    IlluminationBufferFinalDemodulated(uint width, uint height){
        this->width = width;
        this->height = height;
        illuminationBindings.push_back(1);
        illuminationBindings.push_back(25);
        fillImages();
    };

    void fillImages(){
        auto image = vsg::Image::create();
        image->imageType = VK_IMAGE_TYPE_2D;
        image->format = VK_FORMAT_B8G8R8A8_UNORM;
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
        illuminationImages.push_back(vsg::DescriptorImage::create(imageInfo, illuminationBindings[0], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));
    
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
        illuminationImages.push_back(vsg::DescriptorImage::create(imageInfo, illuminationBindings[1], 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE));
    }
};
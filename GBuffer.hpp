#pragma once

#include <vsg/all.h>

// class holding all references for the gbuffer
// supports automatically updating the ray tracing descriptor set
// care to update also the RayTracingVisitor.hpp descriptor layout if you add things to the gbuffer
class GBuffer: public vsg::Inherit<vsg::Object, GBuffer>{
public:
    GBuffer(uint width, uint height): width(width), height(height){setupImages();}

    // these are the bindings the gbuffers are bound to
    const static uint depthBinding = 0, normalBinding = 1, materialBinding = 2, albedoBinding = 3;
    uint width, height;
    vsg::ref_ptr<vsg::DescriptorImage> depth, normal, material, albedo;

    void compile(vsg::Context& context){
        depth->compile(context);
        normal->compile(context);
        material->compile(context);
        albedo->compile(context);
    }

protected:
    void setupImages(){
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
        depth = vsg::DescriptorImage::create(imageInfo, depthBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

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
        normal = vsg::DescriptorImage::create(imageInfo, normalBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

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
        material = vsg::DescriptorImage::create(imageInfo, materialBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

        image = vsg::Image::create();
        image->imageType = VK_IMAGE_TYPE_2D;
        image->format = VK_FORMAT_R8G8B8_UNORM;
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
        albedo = vsg::DescriptorImage::create(imageInfo, albedoBinding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    }
};
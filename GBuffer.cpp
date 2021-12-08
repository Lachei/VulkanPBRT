#include "GBuffer.hpp"

GBuffer::GBuffer(uint32_t width, uint32_t height) : width(width), height(height)
{
    setupImages();
}
void GBuffer::updateDescriptor(vsg::BindDescriptorSet* descSet, const vsg::BindingMap& bindingMap)
{
    vsg::DescriptorSetLayoutBindings& bindings = descSet->descriptorSet->setLayout->bindings;
    int depthInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "depthImage").second;
    depth->dstBinding = depthInd;
    int normalInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "normalImage").second;
    normal->dstBinding = normalInd;
    int materialInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "materialImage").second;
    material->dstBinding = materialInd;
    int albedoInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "albedoImage").second;
    albedo->dstBinding = albedoInd;

    descSet->descriptorSet->descriptors.push_back(depth);
    descSet->descriptorSet->descriptors.push_back(normal);
    descSet->descriptorSet->descriptors.push_back(material);
    descSet->descriptorSet->descriptors.push_back(albedo);
}
void GBuffer::updateImageLayouts(vsg::Context& context)
{
    VkImageSubresourceRange resourceRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto depthLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                       VK_IMAGE_LAYOUT_GENERAL, 0, 0, depth->imageInfoList[0].imageView->image,
                                                       resourceRange);
    auto normalLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                        VK_IMAGE_LAYOUT_GENERAL, 0, 0, normal->imageInfoList[0].imageView->image,
                                                        resourceRange);
    auto materialLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                          VK_IMAGE_LAYOUT_GENERAL, 0, 0, material->imageInfoList[0].imageView->image,
                                                          resourceRange);
    auto albedoLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                        VK_IMAGE_LAYOUT_GENERAL, 0, 0, albedo->imageInfoList[0].imageView->image,
                                                        resourceRange);

    auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                                        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                                        depthLayout, normalLayout, materialLayout, albedoLayout);
    context.commands.push_back(pipelineBarrier);
}
void GBuffer::compile(vsg::Context& context)
{
    depth->compile(context);
    normal->compile(context);
    material->compile(context);
    albedo->compile(context);
}
void GBuffer::setupImages()
{
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
}

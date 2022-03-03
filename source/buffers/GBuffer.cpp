#include <buffers/GBuffer.hpp>

GBuffer::GBuffer(uint32_t width, uint32_t height) : width(width), height(height)
{
    setup_images();
}
void GBuffer::update_descriptor(vsg::BindDescriptorSet* desc_set, const vsg::BindingMap& binding_map) const
{
    vsg::DescriptorSetLayoutBindings& bindings = desc_set->descriptorSet->setLayout->bindings;
    int depth_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "depthImage").second;
    auto depth_bind = vsg::DescriptorImage::create(depth->imageInfoList, depth_ind, 0, depth->descriptorType);
    int normal_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "normalImage").second;
    auto normal_bind = vsg::DescriptorImage::create(normal->imageInfoList, normal_ind, 0, normal->descriptorType);
    int material_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "materialImage").second;
    auto material_bind = vsg::DescriptorImage::create(material->imageInfoList, material_ind, 0, material->descriptorType);
    int albedo_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "albedoImage").second;
    auto albedo_bind = vsg::DescriptorImage::create(albedo->imageInfoList, albedo_ind, 0, albedo->descriptorType);

    desc_set->descriptorSet->descriptors.push_back(depth_bind);
    desc_set->descriptorSet->descriptors.push_back(normal_bind);
    desc_set->descriptorSet->descriptors.push_back(material_bind);
    desc_set->descriptorSet->descriptors.push_back(albedo_bind);
}
void GBuffer::update_image_layouts(vsg::Context& context) const
{
    VkImageSubresourceRange resource_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto depth_layout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                       VK_IMAGE_LAYOUT_GENERAL, 0, 0, depth->imageInfoList[0]->imageView->image,
                                                       resource_range);
    auto normal_layout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                        VK_IMAGE_LAYOUT_GENERAL, 0, 0, normal->imageInfoList[0]->imageView->image,
                                                        resource_range);
    auto material_layout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                          VK_IMAGE_LAYOUT_GENERAL, 0, 0, material->imageInfoList[0]->imageView->image,
                                                          resource_range);
    auto albedo_layout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                                        VK_IMAGE_LAYOUT_GENERAL, 0, 0, albedo->imageInfoList[0]->imageView->image,
                                                        resource_range);

    auto pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                                        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_DEPENDENCY_BY_REGION_BIT,
                                                        depth_layout, normal_layout, material_layout, albedo_layout);
    context.commands.emplace_back(pipeline_barrier);
}
void GBuffer::compile(vsg::Context& context) const
{
    depth->compile(context);
    normal->compile(context);
    material->compile(context);
    albedo->compile(context);
}
void GBuffer::setup_images()
{
    _sampler = vsg::Sampler::create();

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
    auto image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    auto image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    depth = vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

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
    image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    normal = vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

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
    image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    material = vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

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
    image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    albedo = vsg::DescriptorImage::create(image_info, 0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
}

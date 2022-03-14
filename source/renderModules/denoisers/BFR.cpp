#include <renderModules/denoisers/BFR.hpp>
#include <renderModules/PipelineStructs.hpp>

#include <string>

BFR::BFR(uint32_t width, uint32_t height, uint32_t work_width, uint32_t work_height, vsg::ref_ptr<GBuffer> g_buffer,
    vsg::ref_ptr<IlluminationBuffer> illu_buffer, vsg::ref_ptr<AccumulationBuffer> acc_buffer)
    : _width(width),
      _height(height),
      _work_width(work_width),
      _work_height(work_height),
      _g_buffer(g_buffer),
      _sampler(vsg::Sampler::create())
{
    if (!illu_buffer.cast<IlluminationBufferDemodulated>() && !illu_buffer.cast<IlluminationBufferDemodulatedFloat>())
    {
        std::cout << "Illumination Buffer type is required to be IlluminationBufferDemodulated/Float for BMFR"
                  << std::endl;
        return;  // demodulated illumination needed
    }
    auto illumination = illu_buffer;
    // adding usage bits to illumination buffer
    illumination->illumination_images[0]->imageInfoList[0]->imageView->image->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    std::string shader_path = "shaders/bfr.comp.spv";
    auto compute_stage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", shader_path);
    compute_stage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::intValue::create(width)      },
        {1, vsg::intValue::create(height)     },
        {2, vsg::intValue::create(work_width) },
        {3, vsg::intValue::create(work_height)}
    };

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
    auto image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_view->viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    auto image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    _accumulated_illumination
        = vsg::DescriptorImage::create(image_info, _denoised_illu_binding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    image_info->sampler = _sampler;
    _sampled_acc_illu = vsg::DescriptorImage::create(
        image_info, _sampled_den_illu_binding, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = VK_FORMAT_B8G8R8A8_UNORM;
    image->extent.width = width;
    image->extent.height = height;
    image->extent.depth = 1;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->samples = VK_SAMPLE_COUNT_1_BIT;
    image->tiling = VK_IMAGE_TILING_OPTIMAL;
    image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_view = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    image_info = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, image_view, VK_IMAGE_LAYOUT_GENERAL);
    _final_illumination = vsg::DescriptorImage::create(image_info, _final_binding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    // descriptor set layout
    auto binding_map = compute_stage->getDescriptorSetLayoutBindingsMap();
    auto descriptor_set_layout = vsg::DescriptorSetLayout::create(binding_map.begin()->second.bindings);
    auto illumination_info = illumination->illumination_images[0]->imageInfoList[0];
    illumination_info->sampler = _sampler;
    // filling descriptor set
    vsg::Descriptors descriptors{vsg::DescriptorImage::create(g_buffer->depth->imageInfoList[0], _depth_binding, 0,
                                     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
        vsg::DescriptorImage::create(
            g_buffer->normal->imageInfoList[0], _normal_binding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
        vsg::DescriptorImage::create(
            g_buffer->material->imageInfoList[0], _material_binding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
        vsg::DescriptorImage::create(
            g_buffer->albedo->imageInfoList[0], _albedo_binding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
        vsg::DescriptorImage::create(
            acc_buffer->motion->imageInfoList[0], _motion_binding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
        vsg::DescriptorImage::create(
            acc_buffer->spp->imageInfoList[0], _sample_binding, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE),
        vsg::DescriptorImage::create(illumination_info, _noisy_binding, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER),
        _accumulated_illumination, _final_illumination, _sampled_acc_illu};
    auto descriptor_set = vsg::DescriptorSet::create(descriptor_set_layout, descriptors);

    VkPushConstantRange push_constant_range;
    push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(RayTracingPushConstants);
    auto pipeline_layout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts{descriptor_set_layout}, vsg::PushConstantRanges{push_constant_range});
    _bind_descriptor_set
        = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, descriptor_set);

    _bfr_pipeline = vsg::ComputePipeline::create(pipeline_layout, compute_stage);
    _bind_bfr_pipeline = vsg::BindComputePipeline::create(_bfr_pipeline);
}
void BFR::compile(vsg::Context& context)
{
    _accumulated_illumination->compile(context);
    _sampled_acc_illu->compile(context);
    _final_illumination->compile(context);
}
void BFR::update_image_layouts(vsg::Context& context)
{
    VkImageSubresourceRange resource_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 2};
    auto acc_illu_layout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, 0, 0,
        _accumulated_illumination->imageInfoList[0]->imageView->image, resource_range);
    resource_range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    auto final_ilu_layout
        = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_GENERAL, 0, 0, _final_illumination->imageInfoList[0]->imageView->image, resource_range);

    auto pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, acc_illu_layout, final_ilu_layout);
    context.commands.emplace_back(pipeline_barrier);
}
void BFR::add_dispatch_to_command_graph(
    vsg::ref_ptr<vsg::Commands> command_graph, vsg::ref_ptr<vsg::PushConstants> push_constants)
{
    command_graph->addChild(_bind_bfr_pipeline);
    command_graph->addChild(_bind_descriptor_set);
    command_graph->addChild(push_constants);
    command_graph->addChild(vsg::Dispatch::create((_width / _work_width + 2), (_height / _work_height + 2), 1));
    auto pipeline_barrier = vsg::PipelineBarrier::create(
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT);
    command_graph->addChild(pipeline_barrier);  // barrier to wait for completion of denoising before taa is applied
}
vsg::ref_ptr<vsg::DescriptorImage> BFR::get_final_descriptor_image() const
{
    return _final_illumination;
}

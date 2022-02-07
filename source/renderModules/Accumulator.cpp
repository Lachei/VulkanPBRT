#include <renderModules/Accumulator.hpp>

Accumulator::Accumulator(vsg::ref_ptr<GBuffer> g_buffer, vsg::ref_ptr<IlluminationBuffer> illuminationBuffer,
    DoubleMatrices& matrices, int workWidth, int workHeight)
    : _width(g_buffer->depth->imageInfoList[0]->imageView->image->extent.width),
      _height(g_buffer->depth->imageInfoList[0]->imageView->image->extent.height),
      accumulated_illumination(IlluminationBufferDemodulated::create(_width, _height)),
      accumulation_buffer(AccumulationBuffer::create(_width, _height)),
      _work_width(workWidth),
      _work_height(workHeight),
      _original_illumination(illuminationBuffer),
      _matrices(matrices)
{
    auto compute_stage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", _shader_path);
    compute_stage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::intValue::create(workWidth) },
        {1, vsg::intValue::create(workHeight)}
    };

    auto binding_map = compute_stage->getDescriptorSetLayoutBindingsMap();
    auto descriptor_set_layout = vsg::DescriptorSetLayout::create(binding_map.begin()->second.bindings);
    auto pipeline_layout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts{descriptor_set_layout}, compute_stage->getPushConstantRanges());
    auto pipeline = vsg::ComputePipeline::create(pipeline_layout, compute_stage);
    _bind_pipeline = vsg::BindComputePipeline::create(pipeline);

    // adding image usage bit for illumination buffer type
    illuminationBuffer->illuminationImages[0]->imageInfoList[0]->imageView->image->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    vsg::Descriptors descriptors;
    auto image_info = vsg::ImageInfo::create(vsg::Sampler::create(),
        illuminationBuffer->illuminationImages[0]->imageInfoList[0]->imageView, VK_IMAGE_LAYOUT_GENERAL);
    int src_index = vsg::ShaderStage::getSetBindingIndex(binding_map, "srcImage").second;
    auto descriptor_image = vsg::DescriptorImage::create(image_info, src_index);
    descriptors.push_back(descriptor_image);
    auto descriptor_set = vsg::DescriptorSet::create(descriptor_set_layout, descriptors);
    _bind_descriptor_set
        = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, descriptor_set);

    g_buffer->updateDescriptor(_bind_descriptor_set, binding_map);
    accumulation_buffer->updateDescriptor(_bind_descriptor_set, binding_map);
    accumulated_illumination->updateDescriptor(_bind_descriptor_set, binding_map);

    _push_constants_value = PCValue::create();
    _push_constants_value->value().view = matrices[_frame_index].view;
    _push_constants_value->value().inv_view = matrices[_frame_index].invView;
    _push_constants_value->value().frame_number = _frame_index;
    _push_constants = vsg::PushConstants::create(VK_SHADER_STAGE_COMPUTE_BIT, 0, _push_constants_value);
}
void Accumulator::compile_images(vsg::Context& context)
{
    accumulation_buffer->compile(context);
    accumulated_illumination->compile(context);
}
void Accumulator::update_image_layouts(vsg::Context& context)
{
    accumulation_buffer->updateImageLayouts(context);
    accumulated_illumination->updateImageLayouts(context);
}
void Accumulator::add_dispatch_to_command_graph(vsg::ref_ptr<vsg::Commands> command_graph)
{
    command_graph->addChild(_bind_pipeline);
    command_graph->addChild(_bind_descriptor_set);
    command_graph->addChild(_push_constants);
    command_graph->addChild(
        vsg::Dispatch::create(uint32_t(ceil(static_cast<float>(_width) / static_cast<float>(_work_width))),
            uint32_t(ceil(static_cast<float>(_height) / static_cast<float>(_work_height))), 1));
}
void Accumulator::set_frame_index(int frame)
{
    _frame_index = frame;
    _push_constants_value->value().view = _matrices[_frame_index].view;
    _push_constants_value->value().inv_view = _matrices[_frame_index].invView;
    if (_frame_index != 0)
    {
        _push_constants_value->value().prev_view = _matrices[_frame_index - 1].view;
        _push_constants_value->value().prev_pos = _matrices[_frame_index - 1].invView[2];
        _push_constants_value->value().prev_pos /= _push_constants_value->value().prev_pos.w;
    }
    _push_constants_value->value().frame_number = _frame_index;
}

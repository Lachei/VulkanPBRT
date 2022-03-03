#include <renderModules/Accumulator.hpp>
#include <vsgXchange/glsl.h>

Accumulator::Accumulator(vsg::ref_ptr<GBuffer> g_buffer, vsg::ref_ptr<IlluminationBuffer> illumination_buffer,
    bool separate_matrices, int workWidth, int workHeight)
    : _width(g_buffer->depth->imageInfoList[0]->imageView->image->extent.width),
      _height(g_buffer->depth->imageInfoList[0]->imageView->image->extent.height),
      accumulated_illumination(IlluminationBufferDemodulated::create(_width, _height)),
      accumulation_buffer(AccumulationBuffer::create(_width, _height)),
      _work_width(workWidth),
      _work_height(workHeight),
      _original_illumination(illumination_buffer),
      _separate_matrices(separate_matrices)
{
    auto options = vsg::Options::create(vsgXchange::glsl::create());
    auto compute_stage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", _shader_path, options);
    if (!compute_stage)
    {
        throw vsg::Exception{"Accumulator::create() could not open compute shader stage"};
    }
    compute_stage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::intValue::create(workWidth) },
        {1, vsg::intValue::create(workHeight)}
    };
    if (separate_matrices)
    {
        auto compile_hints = vsg::ShaderCompileSettings::create();
        compile_hints->defines = {"SEPARATE_MATRICES"};
        compute_stage->module->hints = compile_hints;
    }

    auto binding_map = compute_stage->getDescriptorSetLayoutBindingsMap();
    auto descriptor_set_layout = vsg::DescriptorSetLayout::create(binding_map.begin()->second.bindings);
    auto pipeline_layout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts{descriptor_set_layout}, compute_stage->getPushConstantRanges());
    auto pipeline = vsg::ComputePipeline::create(pipeline_layout, compute_stage);
    _bind_pipeline = vsg::BindComputePipeline::create(pipeline);

    // adding image usage bit for illumination buffer type
    illumination_buffer->illumination_images[0]->imageInfoList[0]->imageView->image->usage
        |= VK_IMAGE_USAGE_SAMPLED_BIT;
    vsg::Descriptors descriptors;
    auto image_info = vsg::ImageInfo::create(vsg::Sampler::create(),
        illumination_buffer->illumination_images[0]->imageInfoList[0]->imageView, VK_IMAGE_LAYOUT_GENERAL);
    int src_index = vsg::ShaderStage::getSetBindingIndex(binding_map, "srcImage").second;
    auto descriptor_image = vsg::DescriptorImage::create(image_info, src_index);
    descriptors.push_back(descriptor_image);
    auto descriptor_set = vsg::DescriptorSet::create(descriptor_set_layout, descriptors);
    _bind_descriptor_set
        = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, descriptor_set);

    g_buffer->update_descriptor(_bind_descriptor_set, binding_map);
    accumulation_buffer->update_descriptor(_bind_descriptor_set, binding_map);
    accumulated_illumination->update_descriptor(_bind_descriptor_set, binding_map);

    _push_constants_value = PCValue::create();
    _push_constants = vsg::PushConstants::create(VK_SHADER_STAGE_COMPUTE_BIT, 0, _push_constants_value);
}

void Accumulator::compile_images(vsg::Context& context) const
{
    accumulation_buffer->compile(context);
    accumulated_illumination->compile(context);
}

void Accumulator::update_image_layouts(vsg::Context& context) const
{
    accumulation_buffer->update_image_layouts(context);
    accumulated_illumination->update_image_layouts(context);
}

void Accumulator::add_dispatch_to_command_graph(vsg::ref_ptr<vsg::Commands> command_graph)
{
    auto pipeline_barrier
        = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0);
    command_graph->addChild(_bind_pipeline);
    command_graph->addChild(_bind_descriptor_set);
    command_graph->addChild(_push_constants);
    command_graph->addChild(
        vsg::Dispatch::create(uint32_t(ceil(static_cast<float>(_width) / static_cast<float>(_work_width))),
            uint32_t(ceil(static_cast<float>(_height) / static_cast<float>(_work_height))), 1));
    command_graph->addChild(pipeline_barrier);
}

void Accumulator::set_camera_matrices(int frame_index, const CameraMatrices& cur, const CameraMatrices& prev)
{
    if (_separate_matrices)
    {
        if (!cur.proj || !cur.inv_proj)
        {
            throw vsg::Exception{
                "Accumulator::setDoubleMatrix(...) Accumulator was created with separateMatrices = true, but "
                "DubleMatrix is missing seperate matrices"};
        }
        _push_constants_value->value().view = cur.inv_proj.value();
        _push_constants_value->value().inv_view = cur.inv_view;
        if (frame_index != 0)
        {
            _push_constants_value->value().prev_view = prev.view;
            _push_constants_value->value().prev_pos = inverse(prev.view)[3];
            _push_constants_value->value().prev_pos.w = 1;
        }
        _push_constants_value->value().frame_number = frame_index;
    }
    else
    {
        _push_constants_value->value().view = cur.view;
        _push_constants_value->value().inv_view = cur.inv_view;
        if (frame_index != 0)
        {
            _push_constants_value->value().prev_view = prev.view;
            _push_constants_value->value().prev_pos = prev.inv_view[2];
            _push_constants_value->value().prev_pos /= _push_constants_value->value().prev_pos.w;
        }
        _push_constants_value->value().frame_number = frame_index;
    }
}

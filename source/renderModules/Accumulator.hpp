#pragma once
#include <vsg/all.h>
#include <io/RenderIO.hpp>
#include <buffers/GBuffer.hpp>
#include <buffers/IlluminationBuffer.hpp>
#include <buffers/AccumulationBuffer.hpp>

class Accumulator : public vsg::Inherit<vsg::Object, Accumulator>
{
private:
    int _width, _height;

public:
    // Always takes the first image in the illumination buffer and accumulates it
    Accumulator(vsg::ref_ptr<GBuffer> g_buffer, vsg::ref_ptr<IlluminationBuffer> illumination_buffer,
        DoubleMatrices& matrices, int work_width = 16, int work_height = 16);

    void compile_images(vsg::Context& context);
    void update_image_layouts(vsg::Context& context);
    void add_dispatch_to_command_graph(vsg::ref_ptr<vsg::Commands> command_graph);
    // Frameindex is needed to upload the correct matrix
    void set_frame_index(int frame);

    vsg::ref_ptr<IlluminationBuffer> accumulated_illumination;
    vsg::ref_ptr<AccumulationBuffer> accumulation_buffer;

private:
    struct PushConstants
    {
        vsg::mat4 view, inv_view, prev_view;
        vsg::vec4 prev_pos;
        int frame_number;
    };
    class PCValue : public Inherit<vsg::Value<PushConstants>, PCValue>
    {
    public:
        PCValue() {}
    };
    std::string _shader_path = "shaders/accumulator.comp.spv";
    int _work_width, _work_height;
    vsg::ref_ptr<GBuffer> _g_buffer;
    vsg::ref_ptr<IlluminationBuffer> _original_illumination;
    DoubleMatrices _matrices;
    int _frame_index = 0;
    vsg::ref_ptr<vsg::BindComputePipeline> _bind_pipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> _bind_descriptor_set;
    vsg::ref_ptr<vsg::PushConstants> _push_constants;
    vsg::ref_ptr<PCValue> _push_constants_value;
};
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
    Accumulator(vsg::ref_ptr<GBuffer> g_buffer, vsg::ref_ptr<IlluminationBuffer> illumination_buffer, bool separate_matrices, int work_width = 16, int work_height = 16);

    void compile_images(vsg::Context &context) const;
    void update_image_layouts(vsg::Context &context) const;
    void add_dispatch_to_command_graph(vsg::ref_ptr<vsg::Commands> command_graph);
    // Frameindex is needed to upload the correct matrix
    void set_camera_matrices(int frame_index, const CameraMatrices& cur, const CameraMatrices& prev);

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
        PCValue()= default;
    };
    std::string _shader_path = "shaders/accumulator.comp";    //normal glsl file has to be loaded as the shader has to be adopted to separateMatrices style
    int _work_width, _work_height;
    vsg::ref_ptr<GBuffer> _g_buffer;
    vsg::ref_ptr<IlluminationBuffer> _original_illumination;
    vsg::ref_ptr<vsg::BindComputePipeline> _bind_pipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> _bind_descriptor_set;
    vsg::ref_ptr<vsg::PushConstants> _push_constants;
    vsg::ref_ptr<PCValue> _push_constants_value;
    bool _separate_matrices;
};
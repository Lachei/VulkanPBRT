#pragma once
#include <buffers/AccumulationBuffer.hpp>
#include <buffers/GBuffer.hpp>
#include <buffers/IlluminationBuffer.hpp>

#include <vsg/all.h>

class BFR : public vsg::Inherit<vsg::Object, BFR>
{
public:
    BFR(uint32_t width, uint32_t height, uint32_t work_width, uint32_t work_height, vsg::ref_ptr<GBuffer> g_buffer,
        vsg::ref_ptr<IlluminationBuffer> illu_buffer, vsg::ref_ptr<AccumulationBuffer> acc_buffer);

    void compile(vsg::Context& context);
    void update_image_layouts(vsg::Context& context);
    void add_dispatch_to_command_graph(
        vsg::ref_ptr<vsg::Commands> command_graph, vsg::ref_ptr<vsg::PushConstants> push_constants);
    vsg::ref_ptr<vsg::DescriptorImage> get_final_descriptor_image() const;

private:
    uint32_t _width, _height, _work_width, _work_height;
    vsg::ref_ptr<GBuffer> _g_buffer;

    uint32_t _depth_binding = 0;
    uint32_t _normal_binding = 1;
    uint32_t _material_binding = 2;
    uint32_t _albedo_binding = 3;
    uint32_t _prev_depth_binding = -1;
    uint32_t _prev_normal_binding = -1;
    uint32_t _motion_binding = 4;
    uint32_t _sample_binding = 5;
    uint32_t _sampled_den_illu_binding = 6;
    uint32_t _denoised_illu_binding = 9;
    uint32_t _final_binding = 7;
    uint32_t _noisy_binding = 8;
    uint32_t _denoised_binding = 9;

    vsg::ref_ptr<vsg::ComputePipeline> _bfr_pipeline;
    vsg::ref_ptr<vsg::BindComputePipeline> _bind_bfr_pipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> _bind_descriptor_set;

    vsg::ref_ptr<vsg::DescriptorImage> _accumulated_illumination, _sampled_acc_illu, _final_illumination;

    vsg::ref_ptr<vsg::Sampler> _sampler;
};

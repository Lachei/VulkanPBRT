#pragma once
#include <buffers/GBuffer.hpp>
#include <buffers/AccumulationBuffer.hpp>

#include <vsg/all.h>

#include <cstdint>

class Taa : public vsg::Inherit<vsg::Object, Taa>
{
public:
    uint32_t denoised_binding = 1, motion_binding = 0, final_image_binding = 2, accumulation_binding = 3;

    Taa(uint32_t width, uint32_t height, uint32_t work_width, uint32_t work_height, vsg::ref_ptr<GBuffer> g_buffer,
        vsg::ref_ptr<AccumulationBuffer> acc_buffer, vsg::ref_ptr<vsg::DescriptorImage> denoised);

    void compile(vsg::Context& context);
    void update_image_layouts(vsg::Context& context);
    void add_dispatch_to_command_graph(vsg::ref_ptr<vsg::Commands> command_graph);
    vsg::ref_ptr<vsg::DescriptorImage> get_final_descriptor_image() const;

private:
    void copy_final_image(vsg::ref_ptr<vsg::Commands> commands, vsg::ref_ptr<vsg::Image> dst_image);

    uint32_t _width, _height, _work_width, _work_height;

    vsg::ref_ptr<vsg::ComputePipeline> _pipeline;
    vsg::ref_ptr<vsg::BindComputePipeline> _bind_pipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> _bind_descriptor_set;
    vsg::ref_ptr<vsg::DescriptorImage> _final_image, _accumulation_image;

    vsg::ref_ptr<vsg::Sampler> _sampler;
};
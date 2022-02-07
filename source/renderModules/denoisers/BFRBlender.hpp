#pragma once
#include <vsg/all.h>

#include <cstdint>

class BFRBlender: public vsg::Inherit<vsg::Object, BFRBlender>{
public:
    BFRBlender(uint32_t width, uint32_t height,
               vsg::ref_ptr<vsg::DescriptorImage> average_image, vsg::ref_ptr<vsg::DescriptorImage> average_squared_image,
               vsg::ref_ptr<vsg::DescriptorImage> denoised0, vsg::ref_ptr<vsg::DescriptorImage> denoised1,
               vsg::ref_ptr<vsg::DescriptorImage> denoised2,
               uint32_t work_width = 16, uint32_t work_height = 16, uint32_t filter_radius = 2);

    void compile(vsg::Context& context);
    void update_image_layouts(vsg::Context& context);
    void add_dispatch_to_command_graph(vsg::ref_ptr<vsg::Commands> command_graph);
    void copy_final_image(vsg::ref_ptr<vsg::Commands> commands, vsg::ref_ptr<vsg::Image> dst_image);
    vsg::ref_ptr<vsg::DescriptorImage> get_final_descriptor_image() const;
private:
    uint32_t _average_binding = 0, _average_squared_binding = 1, _denoised0_binding = 2, _denoised1_binding = 3, _denoised2_binding = 4, _final_binding = 5;
    vsg::ref_ptr<vsg::DescriptorImage> _final_image;
    vsg::ref_ptr<vsg::ComputePipeline> _pipeline;
    vsg::ref_ptr<vsg::BindComputePipeline> _bind_pipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> _bind_descriptor_set;
    uint32_t _width, _height, _work_width, _work_height, _filter_radius;
};

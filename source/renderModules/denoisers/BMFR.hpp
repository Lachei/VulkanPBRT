#pragma once

#include <renderModules/Taa.hpp>
#include <buffers/IlluminationBuffer.hpp>

#include <vsg/all.h>

// Reimplementation of the Blockwise multi order feature regression method presented in https://webpages.tuni.fi/foi/papers/Koskela-TOG-2019-Blockwise_Multi_Order_Feature_Regression_for_Real_Time_Path_Tracing_Reconstruction.pdf
// This version is converted to vulkan and optimized, such that it uses less kernels, and does require accumulated images to be given to it.
// Further the spacial features used for feature fitting are in screen space to reduce calculation efforts.


class BMFR: public vsg::Inherit<vsg::Object, BMFR>{
public:
    BMFR(uint32_t width, uint32_t height, uint32_t work_width, uint32_t work_height, vsg::ref_ptr<GBuffer> g_buffer,
         vsg::ref_ptr<IlluminationBuffer> illu_buffer, vsg::ref_ptr<AccumulationBuffer> acc_buffer, uint32_t fitting_kernel = 256);

    void compile(vsg::Context& context);
    void update_image_layouts(vsg::Context& context);
    void add_dispatch_to_command_graph(vsg::ref_ptr<vsg::Commands> command_graph, vsg::ref_ptr<vsg::PushConstants> push_constants);
    vsg::ref_ptr<vsg::DescriptorImage> get_final_descriptor_image() const;
private:
    uint32_t _depth_binding = 0, _normal_binding = 1, _material_binding = 2, _albedo_binding = 3, _motion_binding = 4, _sample_binding = 5, _sampled_den_illu_binding = 6, _final_binding = 7, _noisy_binding = 8, _denoised_binding = 9, _feature_buffer_binding = 10, _weights_binding = 11;
    uint32_t _amt_of_features = 13;

    uint32_t _width, _height, _work_width, _work_height, _fitting_kernel, _width_padded, _height_padded;
    vsg::ref_ptr<GBuffer> _g_buffer;
    vsg::ref_ptr<vsg::Sampler> _sampler;
    vsg::ref_ptr<vsg::BindComputePipeline> _bind_pre_pipeline, _bind_fit_pipeline, _bind_post_pipeline;
    vsg::ref_ptr<vsg::ComputePipeline> _bmfr_pre_pipeline, _bmfr_fit_pipeline, _bmfr_post_pipeline;
    vsg::ref_ptr<vsg::DescriptorImage> _accumulated_illumination, _final_illumination, _feature_buffer, _r_mat, _weights;
    vsg::ref_ptr<vsg::BindDescriptorSet> _bind_descriptor_set;
};


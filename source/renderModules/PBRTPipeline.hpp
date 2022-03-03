#pragma once

#include <buffers/GBuffer.hpp>
#include <buffers/IlluminationBuffer.hpp>
#include <scene/RayTracingVisitor.hpp>
#include <buffers/AccumulationBuffer.hpp>

#include <vsg/all.h>
#include <vsgXchange/glsl.h>

#include <cstdint>

enum class RayTracingRayOrigin
{
    CAMERA,
    GBUFFER,
};

class PBRTPipeline : public vsg::Inherit<vsg::Object, PBRTPipeline>
{
public:
    PBRTPipeline(vsg::ref_ptr<vsg::Node> scene, vsg::ref_ptr<GBuffer> g_buffer,
                 vsg::ref_ptr<IlluminationBuffer> illumination_buffer, bool write_g_buffer, RayTracingRayOrigin ray_tracing_ray_origin);

    void set_tlas(vsg::ref_ptr<vsg::AccelerationStructure> as);
    void compile(vsg::Context& context);
    void update_image_layouts(vsg::Context& context);
    void add_trace_rays_to_command_graph(vsg::ref_ptr<vsg::Commands> command_graph, vsg::ref_ptr<vsg::PushConstants> push_constants);
    vsg::ref_ptr<IlluminationBuffer> get_illumination_buffer() const;
    enum class LightSamplingMethod{
        SAMPLE_SURFACE_STRENGTH,
        SAMPLE_LIGHT_STRENGTH,
        SAMPLE_UNIFORM
    }light_sampling_method = LightSamplingMethod::SAMPLE_SURFACE_STRENGTH;
private:
    void setup_pipeline(vsg::Node* scene, bool use_external_gbuffer);
    vsg::ref_ptr<vsg::ShaderStage> setup_raygen_shader(std::string raygen_path, bool use_external_g_buffer);

    std::vector<bool> _opaque_geometries;
    uint32_t _width, _height, _max_recursion_depth, _sample_per_pixel;

    // TODO: add buffers here
    vsg::ref_ptr<GBuffer> _g_buffer;
    vsg::ref_ptr<IlluminationBuffer> _illumination_buffer;

    //resources which have to be added as childs to a scenegraph for rendering
    vsg::ref_ptr<vsg::BindRayTracingPipeline> _bind_ray_tracing_pipeline;
    vsg::ref_ptr<vsg::BindDescriptorSet> _bind_ray_tracing_descriptor_set;
    vsg::ref_ptr<vsg::PushConstants> _push_constants;

    //shader binding table for trace rays
    vsg::ref_ptr<vsg::RayTracingShaderBindingTable> _shader_binding_table;

    //binding map containing all descriptor bindings in the shaders
    vsg::BindingMap _binding_map;
};

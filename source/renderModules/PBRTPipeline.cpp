#include <renderModules/PBRTPipeline.hpp>

#include <cassert>

namespace
{
struct ConstantInfos
{
    uint32_t light_count;
    float light_strength_sum;
    uint32_t min_recursion_depth;
    uint32_t max_recursion_depth;
};

class ConstantInfosValue : public vsg::Inherit<vsg::Value<ConstantInfos>, ConstantInfosValue>
{
public:
    ConstantInfosValue() = default;
};
}  // namespace

PBRTPipeline::PBRTPipeline(vsg::ref_ptr<vsg::Node> scene, vsg::ref_ptr<GBuffer> g_buffer,
    vsg::ref_ptr<IlluminationBuffer> illumination_buffer, bool write_g_buffer,
    RayTracingRayOrigin ray_tracing_ray_origin)
    : _width(illumination_buffer->illumination_images[0]->imageInfoList[0]->imageView->image->extent.width),
      _height(illumination_buffer->illumination_images[0]->imageInfoList[0]->imageView->image->extent.height),
      _max_recursion_depth(2),
      _g_buffer(g_buffer),
      _illumination_buffer(illumination_buffer)
{
    if (write_g_buffer)
    {
        assert(gBuffer);
    }
    bool use_external_g_buffer = ray_tracing_ray_origin == RayTracingRayOrigin::GBUFFER;
    setup_pipeline(scene, use_external_g_buffer);
}
void PBRTPipeline::set_tlas(vsg::ref_ptr<vsg::AccelerationStructure> as)
{
    auto tlas = as.cast<vsg::TopLevelAccelerationStructure>();
    assert(tlas);
    for (int i = 0; i < tlas->geometryInstances.size(); ++i)
    {
        if (_opaque_geometries[i])
        {
            tlas->geometryInstances[i]->shaderOffset = 0;
        }
        else
        {
            tlas->geometryInstances[i]->shaderOffset = 1;
        }
        tlas->geometryInstances[i]->flags = VK_GEOMETRY_NO_DUPLICATE_ANY_HIT_INVOCATION_BIT_KHR;
    }
    auto accel_descriptor = vsg::DescriptorAccelerationStructure::create(vsg::AccelerationStructures{as}, 0, 0);
    _bind_ray_tracing_descriptor_set->descriptorSet->descriptors.push_back(accel_descriptor);
}
void PBRTPipeline::compile(vsg::Context& context)
{
    _illumination_buffer->compile(context);
}
void PBRTPipeline::update_image_layouts(vsg::Context& context)
{
    _illumination_buffer->update_image_layouts(context);
}
void PBRTPipeline::add_trace_rays_to_command_graph(
    vsg::ref_ptr<vsg::Commands> command_graph, vsg::ref_ptr<vsg::PushConstants> push_constants)
{
    auto pipeline_barrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT);
    command_graph->addChild(_bind_ray_tracing_pipeline);
    command_graph->addChild(_bind_ray_tracing_descriptor_set);
    command_graph->addChild(push_constants);
    auto trace_rays = vsg::TraceRays::create();
    trace_rays->bindingTable = _shader_binding_table;
    trace_rays->width = _width;
    trace_rays->height = _height;
    trace_rays->depth = 1;
    command_graph->addChild(trace_rays);
    command_graph->addChild(pipeline_barrier);
}
vsg::ref_ptr<IlluminationBuffer> PBRTPipeline::get_illumination_buffer() const
{
    return _illumination_buffer;
}
void PBRTPipeline::setup_pipeline(vsg::Node* scene, bool use_external_gbuffer)
{
    // parsing data from scene
    RayTracingSceneDescriptorCreationVisitor build_descriptor_binding;
    scene->accept(build_descriptor_binding);
    _opaque_geometries = build_descriptor_binding.is_opaque;

    const int max_lights = 800;
    if (build_descriptor_binding.packed_lights.size() > max_lights)
    {
        light_sampling_method = LightSamplingMethod::SAMPLE_UNIFORM;
    }

    // creating the shader stages and shader binding table
    std::string raygen_path = "shaders/ptRaygen.rgen";  // raygen shader not yet precompiled
    std::string raymiss_path = "shaders/ptMiss.rmiss.spv";
    std::string shadow_miss_path = "shaders/shadow.rmiss.spv";
    std::string closesthit_path = "shaders/ptClosesthit.rchit.spv";
    std::string any_hit_path = "shaders/ptAlphaHit.rahit.spv";

    auto raygen_shader = setup_raygen_shader(raygen_path, use_external_gbuffer);
    auto raymiss_shader = vsg::ShaderStage::read(VK_SHADER_STAGE_MISS_BIT_KHR, "main", raymiss_path);
    auto shadow_miss_shader = vsg::ShaderStage::read(VK_SHADER_STAGE_MISS_BIT_KHR, "main", shadow_miss_path);
    auto closesthit_shader = vsg::ShaderStage::read(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "main", closesthit_path);
    auto any_hit_shader = vsg::ShaderStage::read(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, "main", any_hit_path);
    if (!raygen_shader || !raymiss_shader || !closesthit_shader || !shadow_miss_shader || !any_hit_shader)
    {
        throw vsg::Exception{"Error: PBRTPipeline::PBRTPipeline(...) failed to create shader stages."};
    }
    _binding_map = vsg::ShaderStage::mergeBindingMaps({raygen_shader->getDescriptorSetLayoutBindingsMap(),
        raymiss_shader->getDescriptorSetLayoutBindingsMap(), shadow_miss_shader->getDescriptorSetLayoutBindingsMap(),
        closesthit_shader->getDescriptorSetLayoutBindingsMap(), any_hit_shader->getDescriptorSetLayoutBindingsMap()});

    auto descriptor_set_layout = vsg::DescriptorSetLayout::create(_binding_map.begin()->second.bindings);
    // auto rayTracingPipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout},
    // vsg::PushConstantRanges{{VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(RayTracingPushConstants)}});
    auto ray_tracing_pipeline_layout = vsg::PipelineLayout::create(
        vsg::DescriptorSetLayouts{descriptor_set_layout}, raygen_shader->getPushConstantRanges());
    auto shader_stage
        = vsg::ShaderStages{raygen_shader, raymiss_shader, shadow_miss_shader, closesthit_shader, any_hit_shader};
    auto raygen_shader_group = vsg::RayTracingShaderGroup::create();
    raygen_shader_group->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    raygen_shader_group->generalShader = 0;
    auto raymiss_shader_group = vsg::RayTracingShaderGroup::create();
    raymiss_shader_group->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    raymiss_shader_group->generalShader = 1;
    auto shadow_miss_shader_group = vsg::RayTracingShaderGroup::create();
    shadow_miss_shader_group->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    shadow_miss_shader_group->generalShader = 2;
    auto closesthit_shader_group = vsg::RayTracingShaderGroup::create();
    closesthit_shader_group->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    closesthit_shader_group->closestHitShader = 3;
    auto transparenthit_shader_group = vsg::RayTracingShaderGroup::create();
    transparenthit_shader_group->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    transparenthit_shader_group->closestHitShader = 3;
    transparenthit_shader_group->anyHitShader = 4;
    auto shader_groups = vsg::RayTracingShaderGroups{raygen_shader_group, raymiss_shader_group,
        shadow_miss_shader_group, closesthit_shader_group, transparenthit_shader_group};
    _shader_binding_table = vsg::RayTracingShaderBindingTable::create();
    _shader_binding_table->bindingTableEntries.raygenGroups = {raygen_shader_group};
    _shader_binding_table->bindingTableEntries.raymissGroups = {raymiss_shader_group, shadow_miss_shader_group};
    _shader_binding_table->bindingTableEntries.hitGroups = {closesthit_shader_group, transparenthit_shader_group};
    auto pipeline = vsg::RayTracingPipeline::create(
        ray_tracing_pipeline_layout, shader_stage, shader_groups, _shader_binding_table, 1);
    _bind_ray_tracing_pipeline = vsg::BindRayTracingPipeline::create(pipeline);
    auto descriptor_set = vsg::DescriptorSet::create(descriptor_set_layout, vsg::Descriptors{});
    _bind_ray_tracing_descriptor_set = vsg::BindDescriptorSet::create(
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, ray_tracing_pipeline_layout, descriptor_set);

    build_descriptor_binding.update_descriptor(_bind_ray_tracing_descriptor_set, _binding_map);
    // creating the constant infos uniform buffer object
    auto constant_infos = ConstantInfosValue::create();
    constant_infos->value().light_count = build_descriptor_binding.packed_lights.size();
    constant_infos->value().light_strength_sum = build_descriptor_binding.packed_lights.back().inclusiveStrength;
    constant_infos->value().max_recursion_depth = _max_recursion_depth;
    uint32_t uniform_buffer_binding = vsg::ShaderStage::getSetBindingIndex(_binding_map, "Infos").second;
    auto constant_infos_descriptor = vsg::DescriptorBuffer::create(constant_infos, uniform_buffer_binding, 0);
    _bind_ray_tracing_descriptor_set->descriptorSet->descriptors.push_back(constant_infos_descriptor);

    // update the descriptor sets
    _illumination_buffer->update_descriptor(_bind_ray_tracing_descriptor_set, _binding_map);
    if (_g_buffer)
    {
        _g_buffer->update_descriptor(_bind_ray_tracing_descriptor_set, _binding_map);
    }
}
vsg::ref_ptr<vsg::ShaderStage> PBRTPipeline::setup_raygen_shader(std::string raygen_path, bool use_external_g_buffer)
{
    std::vector<std::string> defines;  // needed defines for the correct illumination buffer

    // set different raygen shaders according to state of external gbuffer and illumination buffer type
    if (use_external_g_buffer)
    {
        // TODO: implement things for external gBuffer
        // raygenPath = "shaders/raygen.rgen.spv";
    }
    else
    {
        if (_illumination_buffer.cast<IlluminationBufferFinalFloat>())
        {
            defines.emplace_back("FINAL_IMAGE");
        }
        else if (_illumination_buffer.cast<IlluminationBufferDemodulatedFloat>())
        {
            defines.emplace_back("DEMOD_ILLUMINATION_FLOAT");
        }
        else if (_illumination_buffer.cast<IlluminationBufferFinalDirIndir>())
        {
            // TODO:
        }
        else
        {
            throw vsg::Exception{"Error: PBRTPipeline::setupRaygenShader(...) Illumination buffer not supported."};
        }
    }
    if (_g_buffer)
    {
        defines.emplace_back("GBUFFER");
    }

    switch (light_sampling_method)
    {
    case LightSamplingMethod::SAMPLE_SURFACE_STRENGTH:
        defines.emplace_back("LIGHT_SAMPLE_SURFACE_STRENGTH");
        break;
    case LightSamplingMethod::SAMPLE_LIGHT_STRENGTH:
        defines.emplace_back("LIGHT_SAMPLE_LIGHT_STRENGTH");
        break;
    default:
        break;
    }

    auto options = vsg::Options::create(vsgXchange::glsl::create());
    auto raygen_shader = vsg::ShaderStage::read(VK_SHADER_STAGE_RAYGEN_BIT_KHR, "main", raygen_path, options);
    if (!raygen_shader)
    {
        throw vsg::Exception{"Error: PBRTPipeline::setupRaygenShader() Could not load ray generation shader."};
    }
    auto compile_hints = vsg::ShaderCompileSettings::create();
    compile_hints->vulkanVersion = VK_API_VERSION_1_2;
    compile_hints->target = vsg::ShaderCompileSettings::SPIRV_1_4;
    compile_hints->defines = defines;
    raygen_shader->module->hints = compile_hints;

    return raygen_shader;
}

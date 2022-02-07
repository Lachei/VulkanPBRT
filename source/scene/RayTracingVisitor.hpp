#pragma once

#include <vsg/all.h>
#include <vector>

class RayTracingSceneDescriptorCreationVisitor : public vsg::Visitor
{
public:
    RayTracingSceneDescriptorCreationVisitor();

    // default visiting for standard nodes, simply forward to children
    void apply(Object& object) override;

    // matrix transformation
    void apply(vsg::Transform& t) override;

    // getting the normals, texture coordinates and vertex data
    void apply(vsg::VertexIndexDraw& vid) override;

    // traversing the states and the group
    void apply(vsg::StateGroup& sg) override;

    // getting the texture samplers of the descriptor sets and checking for opaqueness
    void apply(vsg::BindDescriptorSet& bds) override;

    // getting the lights in the scene
    void apply(const vsg::Light& l);

    void update_descriptor(vsg::BindDescriptorSet* desc_set, const vsg::BindingMap& binding_map);

    // holds the binding command for the raytracing decriptor
    std::vector<vsg::Light::PackedLight> packed_lights;
    // holds information about each geometry if it is opaque
    std::vector<bool> is_opaque;

protected:
    struct ObjectInstance
    {
        vsg::mat4 object_mat;
        int mesh_id;  // index of the corresponding textuers, vertices etc.
        uint32_t index_stride;
        int pad[2];
    };

    struct WaveFrontMaterialPacked
    {
        vsg::vec4 ambient_roughness;
        vsg::vec4 diffuse_ior;
        vsg::vec4 specular_dissolve;
        vsg::vec4 transmittance_illum;
        vsg::vec4 emission_texture_id;
        uint32_t category_id;
        float padding[3];
    };

    vsg::ref_ptr<vsg::DescriptorBuffer> _instances;
    std::vector<ObjectInstance> _instances_array;
    // images are available correctly for every instance
    std::vector<vsg::ref_ptr<vsg::DescriptorImage>> _diffuse;
    std::vector<vsg::ref_ptr<vsg::DescriptorImage>> _mr;
    std::vector<vsg::ref_ptr<vsg::DescriptorImage>> _normal;
    std::vector<vsg::ref_ptr<vsg::DescriptorImage>> _emissive;
    std::vector<vsg::ref_ptr<vsg::DescriptorImage>> _specular;
    // buffers are available for each geometry
    std::vector<vsg::ref_ptr<vsg::DescriptorBuffer>> _positions;
    std::vector<vsg::ref_ptr<vsg::DescriptorBuffer>> _normals;
    std::vector<vsg::ref_ptr<vsg::DescriptorBuffer>> _tex_coords;
    std::vector<vsg::ref_ptr<vsg::DescriptorBuffer>> _indices;
    vsg::ref_ptr<vsg::DescriptorBuffer> _materials;
    std::vector<WaveFrontMaterialPacked> _material_array;
    vsg::ref_ptr<vsg::DescriptorBuffer> _lights;

    std::map<vsg::VertexIndexDraw*, ObjectInstance> _vertex_index_draw_map;
    vsg::MatrixStack _transform_stack;

    // the first state group contains the default state which should be skipped
    vsg::ref_ptr<vsg::DescriptorImage> _default_texture;
    // the first state group contains the default state which should be skipped
    bool _first_stage_group = true;
    // set to true by a descriptor set that has emission
    bool _mesh_emissive = false;
};

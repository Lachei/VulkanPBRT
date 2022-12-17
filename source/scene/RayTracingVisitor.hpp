#pragma once

#include <vsg/all.h>
#include <vector>

const uint32_t lst_undefined = 0;
const uint32_t lst_directional = 1;
const uint32_t lst_point = 2;
const uint32_t lst_spot = 3;
const uint32_t lst_ambient = 4;
const uint32_t lst_area = 5;

class RayTracingSceneDescriptorCreationVisitor : public vsg::Visitor
{
public:
    struct PackedLight
    {  // same layout as in shaders/ptSturctures.glsl
        PackedLight() = default;
        PackedLight& operator=(const PackedLight&) = default;
        explicit PackedLight(const vsg::Light& l) {}
        explicit PackedLight(const vsg::AmbientLight& l)
        {
            v0_type.w = lst_ambient;
            col_ambient.xyz = col_diffuse.xyz = col_specular.xyz = l.color;
            strengths.xyz = l.strengths;
        }
        explicit PackedLight(const vsg::DirectionalLight& l)
        {
            v0_type.w = lst_directional;
            col_ambient.xyz = col_diffuse.xyz = col_specular.xyz = l.color;
            dir_angle2.xyz = l.direction;
            strengths.xyz = l.strengths;
        }
        explicit PackedLight(const vsg::PointLight& l)
        {
            v0_type.w = lst_point;
            v0_type.xyz = l.position;
            col_ambient.xyz = col_diffuse.xyz = col_specular.xyz = l.color;
            strengths.xyz = l.strengths;
        }
        explicit PackedLight(const vsg::SpotLight& l) {}
        explicit PackedLight(const vsg::TriangleLight& l)
        {
            v0_type.w = lst_area;
            v0_type.xyz = l.positions[0];
            v1_strength.xyz = l.positions[1];
            v2_angle.xyz = l.positions[2];
            col_ambient.xyz = col_diffuse.xyz = col_specular.xyz = l.color;
            strengths.xyz = l.strengths;
        }

        vsg::vec4 v0_type;
        vsg::vec4 v1_strength;
        vsg::vec4 v2_angle;
        vsg::vec4 dir_angle2;
        vsg::vec4 col_ambient;
        vsg::vec4 col_diffuse;
        vsg::vec4 col_specular;
        vsg::vec4 strengths;
    };

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
    std::vector<PackedLight> packed_lights;
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

    vsg::ref_ptr<vsg::DescriptorImage>
        _default_texture;            // the default image is used for each texture that is not available
    bool _first_stage_group = true;  // the first state group contains the default state which should be skipped
    bool _mesh_emissive = false;     // set to true by a descriptor set that has emission
};

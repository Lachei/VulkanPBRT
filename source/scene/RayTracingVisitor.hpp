#pragma once

#include <vsg/all.h>
#include <vector>

class RayTracingSceneDescriptorCreationVisitor : public vsg::Visitor
{
public:
    RayTracingSceneDescriptorCreationVisitor();
    
    //default visiting for standard nodes, simply forward to children
    void apply(vsg::Object& object);

    //matrix transformation
    void apply(vsg::Transform& t);

    //getting the normals, texture coordinates and vertex data
    void apply(vsg::VertexIndexDraw& vid);

    //traversing the states and the group
    void apply(vsg::StateGroup& sg);

    //getting the texture samplers of the descriptor sets and checking for opaqueness
    void apply(vsg::BindDescriptorSet& bds);

    //getting the lights in the scene
    void apply(const vsg::Light& l);

    void updateDescriptor(vsg::BindDescriptorSet* descSet, const vsg::BindingMap& bindingMap);

    //holds the binding command for the raytracing decriptor
    std::vector<vsg::Light::PackedLight> packedLights;
    //holds information about each geometry if it is opaque
    std::vector<bool> isOpaque;
protected:
    struct ObjectInstance{
        vsg::mat4 objectMat;
        int meshId;         //index of the corresponding textuers, vertices etc.
        uint32_t indexStride;
        int pad[2];
    };
    struct WaveFrontMaterialPacked
    {
        vsg::vec4  ambientRoughness;
        vsg::vec4  diffuseIor;
        vsg::vec4  specularDissolve;
        vsg::vec4  transmittanceIllum;
        vsg::vec4  emissionTextureId;
        uint32_t categoryID;
        float   padding[3];
    };
    vsg::ref_ptr<vsg::DescriptorBuffer> _instances;
    std::vector<ObjectInstance> _instancesArray;
    //images are available correctly for every instance
    std::vector<vsg::ref_ptr<vsg::DescriptorImage>> _diffuse;
    std::vector<vsg::ref_ptr<vsg::DescriptorImage>> _mr;
    std::vector<vsg::ref_ptr<vsg::DescriptorImage>> _normal;
    std::vector<vsg::ref_ptr<vsg::DescriptorImage>> _emissive;
    std::vector<vsg::ref_ptr<vsg::DescriptorImage>> _specular;
    //buffers are available for each geometry
    std::vector<vsg::ref_ptr<vsg::DescriptorBuffer>> _positions;
    std::vector<vsg::ref_ptr<vsg::DescriptorBuffer>> _normals;
    std::vector<vsg::ref_ptr<vsg::DescriptorBuffer>> _texCoords;
    std::vector<vsg::ref_ptr<vsg::DescriptorBuffer>> _indices;
    vsg::ref_ptr<vsg::DescriptorBuffer> _materials;
    std::vector<WaveFrontMaterialPacked> _materialArray;
    vsg::ref_ptr<vsg::DescriptorBuffer> _lights;

    std::map<vsg::VertexIndexDraw*, ObjectInstance> _vertexIndexDrawMap;
    vsg::MatrixStack _transformStack;

    vsg::ref_ptr<vsg::DescriptorImage> _defaultTexture;   //the default image is used for each texture that is not available
    bool firstStageGroup = true;                        //the first state group contains the default state which should be skipped
    bool meshEmissive = false;                          //set to true by a descriptor set that has emission
};


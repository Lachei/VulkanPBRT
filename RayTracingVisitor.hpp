#pragma once

#include "Defines.hpp"
#include <vsg/all.h>
#include <vector>
#include "PipelineStructs.hpp"

class CreateRayTracingDescriptorTraversal : public vsg::Visitor
{
public:
    CreateRayTracingDescriptorTraversal()
    {
        vsg::ubvec4 w{255,255,255,255};
        auto white = vsg::ubvec4Array2D::create(1, 1, vsg::Data::Layout{VK_FORMAT_R8G8B8A8_UNORM});
        std::copy(&w, &w + 1, white->data());

        vsg::SamplerImage image;
        image.data = white;
        image.sampler = vsg::Sampler::create();
        image.sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        image.sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        image.sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        image.sampler->anisotropyEnable = VK_FALSE;
        image.sampler->maxLod = 1;
        _defaultTexture = vsg::DescriptorImage::create(image);
    };
    
    //default visiting for standard nodes, simply forward to children
    void apply(vsg::Object& object){
        object.traverse(*this);
    }

    //matrix transformation
    void apply(vsg::MatrixTransform& mt)
    {
        _transformStack.pushAndPreMult(mt.getMatrix());

        mt.traverse(*this);

        _transformStack.pop();
    }

    //getting the normals, texture coordinates and vertex data
    void apply(vsg::VertexIndexDraw& vid)
    {
        if (vid.arrays.size() == 0) return;

        //check cache
        bool cached = _vertexIndexDrawMap.find(&vid) != _vertexIndexDrawMap.end();
        ObjectInstance instance;
        instance.objectMat = _transformStack.top();
        if(cached) 
        {
            instance.meshId = _vertexIndexDrawMap[&vid].meshId;
            instance.indexStride = _vertexIndexDrawMap[&vid].indexStride;
        }
        else
        {
            instance.meshId = _positions.size();
            _vertexIndexDrawMap[&vid] = instance;
            auto positions = vsg::DescriptorBuffer::create(vid.arrays[0], 2, _positions.size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            _positions.push_back(positions);
            auto normals = vsg::DescriptorBuffer::create(vid.arrays[1], 3, _normals.size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            _normals.push_back(normals);
            auto texCoords = vsg::DescriptorBuffer::create(vid.arrays[2], 4, _texCoords.size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            _texCoords.push_back(texCoords);
            auto indices = vsg::DescriptorBuffer::create(vid.indices, 5, _indices.size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            _indices.push_back(indices);
            instance.indexStride = vid.indices->stride();
        }
        _instancesArray.push_back(instance);
    }

    //traversing the states and the group
    void apply(vsg::StateGroup& sg)
    {
        if(firstStageGroup)     //skip default state grop(the first in the tree) TODO::change to detect default state
            firstStageGroup = false;
        else
        {
            vsg::StateGroup::StateCommands& sc = sg.getStateCommands();
            for(auto& state: sc)
            {
                auto bds = state.cast<vsg::BindDescriptorSet>();
                if(bds){
                    apply(*bds);
                }
            }
        }
        sg.traverse(*this);
    }

    //getting the texture samplers of the descriptor sets and checking for opaqueness
    void apply(vsg::BindDescriptorSet &bds){
        // TODO: every material that is not set should get a default material assigned
        std::set<int> setTextures;
        isOpaque.push_back(true);
        for(const auto& descriptor: bds.descriptorSet->descriptors)
        {
            if(descriptor->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) //pbr material
            {
                //TODO change to automatically detect pbr and normal vsg mat via data stride
                vsg::ref_ptr<vsg::DescriptorBuffer> d = descriptor.cast<vsg::DescriptorBuffer>();
                VsgPbrMaterial vsgMat;
                std::memcpy(&vsgMat, d->bufferInfoList[0].data->dataPointer(), sizeof(VsgPbrMaterial));
                WaveFrontMaterialPacked mat;
                std::memcpy(&mat.ambientShininess, &vsgMat.baseColorFactor, sizeof(vsg::vec4));
                std::memcpy(&mat.specularDissolve, &vsgMat.specularFactor, sizeof(vsg::vec4));
                std::memcpy(&mat.diffuseIor, &vsgMat.diffuseFactor, sizeof(vsg::vec4));
                mat.ambientShininess.w = vsgMat.roughnessFactor;
                mat.diffuseIor.w = 1;
                mat.specularDissolve.w = vsgMat.alphaMask;
                mat.emissionTextureId.w = vsgMat.alphaMaskCutoff;
                _materialArray.push_back(mat);
                continue;
            }

            if(descriptor->descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) continue;

            vsg::ref_ptr<vsg::DescriptorImage> d = descriptor.cast<vsg::DescriptorImage>();  //cast to descriptor image
            vsg::ref_ptr<vsg::DescriptorImage> texture;
            switch(descriptor->dstBinding){
            case 0: //diffuse map
                texture = vsg::DescriptorImage::create(d->imageInfoList, 6, _diffuse.size());
                _diffuse.push_back(texture);
                setTextures.insert(6);
                // check for opaqueness
                {
                    auto data = d->imageInfoList[0].imageView->image->data;
                    //int amt = data->dataSize() / data->stride();
                    for(int i = 0; i < data->dataSize() / data->stride() && isOpaque.back(); ++i){
                        void* d = static_cast<char*>(data->dataPointer()) + i * data->stride();
                        switch(data->getLayout().format){
                            case VK_FORMAT_R32G32B32A32_SFLOAT:
                                if(static_cast<float*>(d)[3] < .01f) isOpaque.back() = false;
                                break;
                            case VK_FORMAT_R8G8B8A8_UNORM:
                            case VK_FORMAT_B8G8R8A8_UNORM:
                                if(static_cast<uint8_t*>(d)[3] < .01f * 255) isOpaque.back() = false;
                                break;
                        }
                    }
                }
                break;
            case 1: //metall roughness map
                texture = vsg::DescriptorImage::create(d->imageInfoList, 7, _mr.size());
                _mr.push_back(texture);
                setTextures.insert(7);
                break;
            case 2: //normal map
                texture = vsg::DescriptorImage::create(d->imageInfoList, 8, _normal.size());
                _normal.push_back(texture);
                setTextures.insert(8);
                break;
            case 3: //light map
                break;
            case 4: //emissive map
                texture = vsg::DescriptorImage::create(d->imageInfoList, 10, _emissive.size());
                _emissive.push_back(texture);
                setTextures.insert(10);
                break;
            case 5: //specular map
                texture = vsg::DescriptorImage::create(d->imageInfoList, 11, _specular.size());
                _specular.push_back(texture);
                setTextures.insert(11);
            default:
                std::cout << "Unkown texture binding. Could not properly detect material" << std::endl;
            }
        }

        //setting the default texture for not set textures
        for(int i = 6; i < 12; ++i){
            if(setTextures.find(i) == setTextures.end()){
                vsg::ref_ptr<vsg::DescriptorImage> texture;
                switch(i){
                case 6:
                    texture = vsg::DescriptorImage::create(_defaultTexture->imageInfoList, i, _diffuse.size());
                    _diffuse.push_back(texture);
                    break;
                case 7:
                    texture = vsg::DescriptorImage::create(_defaultTexture->imageInfoList, i, _mr.size());
                    _mr.push_back(texture);
                    break;
                case 8:
                    texture = vsg::DescriptorImage::create(_defaultTexture->imageInfoList, i, _normal.size());
                    _normal.push_back(texture);
                    break;
                case 9:
                    break;
                case 10:
                    texture = vsg::DescriptorImage::create(_defaultTexture->imageInfoList, i, _emissive.size());
                    _emissive.push_back(texture);
                    break;
                case 11:
                    texture = vsg::DescriptorImage::create(_defaultTexture->imageInfoList, i, _specular.size());
                    _specular.push_back(texture);
                    break;
                }
            }
        }
    }

    //getting the lights in the scene
    void apply(const vsg::Light& l){
        packedLights.push_back(l.getPacked());
    }

    vsg::ref_ptr<vsg::BindDescriptorSet> getBindDescriptorSet(vsg::ref_ptr<vsg::PipelineLayout> pipelineLayout, const vsg::BindingMap& bindingMap){
        if(!_bindDescriptor){
            if(packedLights.empty())
            {
                std::cout << "Adding default directional light for raytracing" << std::endl;
                vsg::Light l;
                l.radius = 0;
                l.type = vsg::LightSourceType::Directional;
                vsg::vec3 col{2.0f,2.0f,2.0f};
                l.colorAmbient = col;
                l.colorDiffuse = col;
                l.colorSpecular = {0,0,0};
                l.strengths = vsg::vec3(.5f,.0f,.0f);
                l.dir = vsg::normalize(vsg::vec3(0.1f, 1, -10.1f));
                packedLights.push_back(l.getPacked());
            }
            if(!_lights)
            {
                auto lights = vsg::Array<vsg::Light::PackedLight>::create(packedLights.size());
                std::copy(packedLights.begin(), packedLights.end(), lights->data());
                _lights = vsg::DescriptorBuffer::create(lights, 12, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            }
            if(!_materials)
            {
                auto materials = vsg::Array<WaveFrontMaterialPacked>::create(_materialArray.size());
                std::copy(_materialArray.begin(), _materialArray.end(), materials->data());
                _materials = vsg::DescriptorBuffer::create(materials, 13, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            }
            if(!_instances)
            {
                auto instances = vsg::Array<ObjectInstance>::create(_instancesArray.size());
                std::copy(_instancesArray.begin(), _instancesArray.end(), instances->data());
                _instances = vsg::DescriptorBuffer::create(instances, 14, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            }

            // setting the descriptor amount for the object arrays
            vsg::DescriptorSetLayoutBindings& bindings = pipelineLayout->setLayouts[0]->bindings;
            int posInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Pos").second;
            std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b){return b.binding == posInd;})->descriptorCount = static_cast<uint32_t>(_positions.size());
            int norInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Nor").second;
            std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b){return b.binding == norInd;})->descriptorCount = static_cast<uint32_t>(_normals.size());
            int texInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Tex").second;
            std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b){return b.binding == texInd;})->descriptorCount = static_cast<uint32_t>(_texCoords.size());
            int indInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Ind").second;
            std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b){return b.binding == indInd;})->descriptorCount = static_cast<uint32_t>(_indices.size());
            int diffuseInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "diffuseMap").second;
            std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b){return b.binding == diffuseInd;})->descriptorCount = static_cast<uint32_t>(_diffuse.size());
            int mrInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "mrMap").second;
            std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b){return b.binding == mrInd;})->descriptorCount = static_cast<uint32_t>(_mr.size());
            int normalInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "normalMap").second;
            std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b){return b.binding == normalInd;})->descriptorCount = static_cast<uint32_t>(_normal.size());
            int emissiveInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "emissiveMap").second;
            std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b){return b.binding == emissiveInd;})->descriptorCount = static_cast<uint32_t>(_emissive.size());
            int specularInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "specularMap").second;
            std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b){return b.binding == specularInd;})->descriptorCount = static_cast<uint32_t>(_specular.size());
            int lightInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Lights").second;
            int matInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Materials").second;
            int instancesInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Instances").second;
            
            //adding all descriptors and updating their binding
            vsg::Descriptors descList;
            for(auto& d: _diffuse){
                d->dstBinding = diffuseInd;
                descList.push_back(d);
            }
            for(auto& d: _mr){
                d->dstBinding = mrInd;
                descList.push_back(d);
            }
            for(auto& d: _normal){
                d->dstBinding = normalInd;
                descList.push_back(d);
            }
            for(auto& d: _emissive){
                d->dstBinding = emissiveInd;
                descList.push_back(d);
            }
            for(auto& d: _specular){
                d->dstBinding = specularInd;
                descList.push_back(d);
            }
            _lights->dstBinding = lightInd;
            _materials->dstBinding = matInd;
            _instances->dstBinding = instancesInd;
            descList.push_back(_lights);
            descList.push_back(_materials);
            descList.push_back(_instances);
            for(auto& d: _positions){
                d->dstBinding = posInd;
                descList.push_back(d);
            }
            for(auto& d: _normals){
                d->dstBinding = norInd;
                descList.push_back(d);
            }
            for(auto& d: _texCoords){
                d->dstBinding = texInd;
                descList.push_back(d);
            }
            for(auto& d: _indices){
                d->dstBinding = indInd;
                descList.push_back(d);
            }
            _descriptorSet = vsg::DescriptorSet::create(pipelineLayout->setLayouts[0], descList);
            _bindDescriptor = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, _descriptorSet);
        }
        return _bindDescriptor;
    };

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
    struct VsgPbrMaterial
    {
        vsg::vec4 baseColorFactor{1.0, 1.0, 1.0, 1.0};
        vsg::vec4 emissiveFactor{0.0, 0.0, 0.0, 1.0};
        vsg::vec4 diffuseFactor{1.0, 1.0, 1.0, 1.0};
        vsg::vec4 specularFactor{0.0, 0.0, 0.0, 1.0};
        float metallicFactor{1.0f};
        float roughnessFactor{1.0f};
        float alphaMask{1.0f};
        float alphaMaskCutoff{0.5f};
    };
    struct WaveFrontMaterialPacked
    {
        vsg::vec4  ambientShininess;
        vsg::vec4  diffuseIor;
        vsg::vec4  specularDissolve;
        vsg::vec4  transmittanceIllum;
        vsg::vec4  emissionTextureId;
    };
    vsg::ref_ptr<vsg::DescriptorBuffer> _instances;
    std::vector<ObjectInstance> _instancesArray;
    vsg::ref_ptr<vsg::DescriptorSetLayout> _descriptorSetLayout;
    vsg::ref_ptr<vsg::BindDescriptorSet> _bindDescriptor;
    vsg::ref_ptr<vsg::DescriptorSet> _descriptorSet;
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
};
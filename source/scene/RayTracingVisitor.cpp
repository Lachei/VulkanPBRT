#include "RayTracingVisitor.hpp"
#include <iostream>

RayTracingSceneDescriptorCreationVisitor::RayTracingSceneDescriptorCreationVisitor()
{
    vsg::ubvec4 w{255, 255, 255, 255};
    auto white = vsg::ubvec4Array2D::create(1, 1, vsg::Data::Layout{VK_FORMAT_R8G8B8A8_UNORM});
    std::copy(&w, &w + 1, white->data());

    auto sampler = vsg::Sampler::create();
    sampler->addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler->addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler->addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler->anisotropyEnable = VK_FALSE;
    sampler->maxLod = 1;
    _default_texture = vsg::DescriptorImage::create(sampler, white, 0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
}
void RayTracingSceneDescriptorCreationVisitor::apply(Object& object)
{
    object.traverse(*this);
}
void RayTracingSceneDescriptorCreationVisitor::apply(vsg::Transform& t)
{
    _transform_stack.push(t);

    t.traverse(*this);

    _transform_stack.pop();
}
void RayTracingSceneDescriptorCreationVisitor::apply(vsg::VertexIndexDraw& vid)
{
    if (vid.arrays.empty())
    {
        return;
    }

    // check cache
    bool cached = _vertex_index_draw_map.find(&vid) != _vertex_index_draw_map.end();
    ObjectInstance instance;
    instance.object_mat = _transform_stack.top();
    if (cached)
    {
        instance.mesh_id = _vertex_index_draw_map[&vid].mesh_id;
        instance.index_stride = _vertex_index_draw_map[&vid].index_stride;
    }
    else
    {
        instance.mesh_id = _positions.size();
        _vertex_index_draw_map[&vid] = instance;
        auto positions = vsg::DescriptorBuffer::create(
            vid.arrays[0]->data, 2, _positions.size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        _positions.push_back(positions);
        std::vector<vsg::vec3> nors(1);
        std::memcpy(nors.data(), vid.arrays[1]->data->dataPointer(), sizeof(vsg::vec3));
        if (length2(nors[0]) == 0)
        {
            // normals have to be computed
            nors.resize(vid.arrays[1]->data->dataSize() / sizeof(nors[0]));
            std::memcpy(nors.data(), vid.arrays[1]->data->dataPointer(), vid.arrays[1]->data->dataSize());
            std::vector<float> weight_sum(nors.size(), 0);
            std::vector<vsg::vec3> positions(vid.arrays[0]->data->dataSize() / sizeof(vsg::vec3));
            std::memcpy(positions.data(), vid.arrays[0]->data->dataPointer(), vid.arrays[0]->data->dataSize());
            if (vid.indices->data->stride() == 4)
            {
                // integer indices
                std::vector<uint32_t> indices(vid.indices->data->dataSize() / sizeof(uint32_t));
                std::memcpy(indices.data(), vid.indices->data->dataPointer(), vid.indices->data->dataSize());
                for (int tri = 0; tri < indices.size() / 3; ++tri)
                {
                    uint32_t ind_a = indices[3 * tri];
                    uint32_t ind_b = indices[3 * tri + 1];
                    uint32_t ind_c = indices[3 * tri + 2];
                    vsg::vec3& a = positions[ind_a];
                    vsg::vec3& b = positions[ind_b];
                    vsg::vec3& c = positions[ind_c];
                    vsg::vec3 face_normal = cross(b - a, c - a);
                    float w = length(face_normal);  // the weight is proportional to the area of the face
                    face_normal /= w;
                    float new_w = w + weight_sum[ind_a];
                    nors[ind_a] = nors[ind_a] * (weight_sum[ind_a] / new_w) + face_normal * (w / new_w);
                    weight_sum[ind_a] = new_w;
                    new_w = w + weight_sum[ind_b];
                    nors[ind_b] = nors[ind_b] * (weight_sum[ind_b] / new_w) + face_normal * (w / new_w);
                    weight_sum[ind_b] = new_w;
                    new_w = w + weight_sum[ind_c];
                    nors[ind_c] = nors[ind_c] * (weight_sum[ind_c] / new_w) + face_normal * (w / new_w);
                    weight_sum[ind_c] = new_w;
                }
            }
            else
            {
                // short indices
                std::vector<uint16_t> indices(vid.indices->data->dataSize() / sizeof(uint16_t));
                std::memcpy(indices.data(), vid.indices->data->dataPointer(), vid.indices->data->dataSize());
                for (int tri = 0; tri < indices.size() / 3; ++tri)
                {
                    uint32_t ind_a = indices[3 * tri];
                    uint32_t ind_b = indices[3 * tri + 1];
                    uint32_t ind_c = indices[3 * tri + 2];
                    vsg::vec3& a = positions[ind_a];
                    vsg::vec3& b = positions[ind_b];
                    vsg::vec3& c = positions[ind_c];
                    vsg::vec3 face_normal = cross(b - a, c - a);
                    float w = length(face_normal);  // the weight is proportional to the area of the face
                    face_normal /= w;
                    float new_w = w + weight_sum[ind_a];
                    nors[ind_a] = nors[ind_a] * (weight_sum[ind_a] / new_w) + face_normal * (w / new_w);
                    weight_sum[ind_a] = new_w;
                    new_w = w + weight_sum[ind_b];
                    nors[ind_b] = nors[ind_b] * (weight_sum[ind_b] / new_w) + face_normal * (w / new_w);
                    weight_sum[ind_b] = new_w;
                    new_w = w + weight_sum[ind_c];
                    nors[ind_c] = nors[ind_c] * (weight_sum[ind_c] / new_w) + face_normal * (w / new_w);
                    weight_sum[ind_c] = new_w;
                }
            }
            // for(auto& normal: nors) normal = vsg::normalize(normal);
            std::memcpy(vid.arrays[1]->data->dataPointer(), nors.data(), vid.arrays[1]->data->dataSize());
        }
        auto normals
            = vsg::DescriptorBuffer::create(vid.arrays[1]->data, 3, _normals.size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        _normals.push_back(normals);
        // auto fill up tex coords if not provided
        vsg::ref_ptr<vsg::DescriptorBuffer> tex_coords;
        if (vid.arrays[2]->data->valueCount() == 0)
        {
            auto data = vsg::vec2Array::create(vid.arrays[0]->data->valueCount());
            for (int i = 0; i < vid.indices->data->valueCount() / 3; ++i)
            {
                uint32_t index = 0;
                if (vid.indices->data->stride() == 2)
                {
                    index = static_cast<uint16_t*>(vid.indices->data->dataPointer())[i * 3];
                }
                else
                {
                    index = static_cast<uint32_t*>(vid.indices->data->dataPointer())[i * 3];
                }
                data->at(index) = vsg::vec2(0, 0);
                if (vid.indices->data->stride() == 2)
                {
                    index = static_cast<uint16_t*>(vid.indices->data->dataPointer())[i * 3 + 1];
                }
                else
                {
                    index = static_cast<uint32_t*>(vid.indices->data->dataPointer())[i * 3 + 1];
                }
                data->at(index) = vsg::vec2(0, 1);
                if (vid.indices->data->stride() == 2)
                {
                    index = static_cast<uint16_t*>(vid.indices->data->dataPointer())[i * 3 + 2];
                }
                else
                {
                    index = static_cast<uint32_t*>(vid.indices->data->dataPointer())[i * 3 + 2];
                }
                data->at(index) = vsg::vec2(1, 0);
            }
            vid.arrays[2]->data = data;
        }
        tex_coords = vsg::DescriptorBuffer::create(
            vid.arrays[2]->data, 4, _tex_coords.size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        _tex_coords.push_back(tex_coords);
        auto indices
            = vsg::DescriptorBuffer::create(vid.indices->data, 5, _indices.size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        _indices.push_back(indices);
        instance.index_stride = vid.indices->data->stride();
    }
    _instances_array.push_back(instance);

    // if emissive mesh create a mesh light for each triangle
    if (_mesh_emissive)
    {
        for (int i = 0; i < vid.indices->data->valueCount() / 3; ++i)
        {
            auto l = vsg::TriangleLight::create();
            l->color = {_material_array.back().emission_texture_id.r, _material_array.back().emission_texture_id.g,
                _material_array.back().emission_texture_id.b};

            uint32_t index = 0;
            if (vid.indices->data->stride() == 2)
            {
                index = static_cast<uint16_t*>(vid.indices->data->dataPointer())[i * 3];
            }
            else
            {
                index = static_cast<uint32_t*>(vid.indices->data->dataPointer())[i * 3];
            }
            l->positions[0] = static_cast<vsg::vec3*>(vid.arrays[0]->data->dataPointer())[index];
            if (vid.indices->data->stride() == 2)
            {
                index = static_cast<uint16_t*>(vid.indices->data->dataPointer())[i * 3 + 1];
            }
            else
            {
                index = static_cast<uint32_t*>(vid.indices->data->dataPointer())[i * 3 + 1];
            }
            l->positions[1] = static_cast<vsg::vec3*>(vid.arrays[0]->data->dataPointer())[index];
            if (vid.indices->data->stride() == 2)
            {
                index = static_cast<uint16_t*>(vid.indices->data->dataPointer())[i * 3 + 2];
            }
            else
            {
                index = static_cast<uint32_t*>(vid.indices->data->dataPointer())[i * 3 + 2];
            }
            l->positions[2] = static_cast<vsg::vec3*>(vid.arrays[0]->data->dataPointer())[index];
            // transforming the light position
            auto t = _transform_stack.top() * vsg::dvec4{l->positions[0].x, l->positions[0].y, l->positions[0].z, 1};
            l->positions[0] = {static_cast<float>(t.x), static_cast<float>(t.y), static_cast<float>(t.z)};
            t = _transform_stack.top() * vsg::dvec4{l->positions[1].x, l->positions[1].y, l->positions[1].z, 1};
            l->positions[1] = {static_cast<float>(t.x), static_cast<float>(t.y), static_cast<float>(t.z)};
            t = _transform_stack.top() * vsg::dvec4{l->positions[2].x, l->positions[2].y, l->positions[2].z, 1};
            l->positions[2] = {static_cast<float>(t.x), static_cast<float>(t.y), static_cast<float>(t.z)};
            packed_lights.emplace_back(*l);
        }
    }
}
void RayTracingSceneDescriptorCreationVisitor::apply(vsg::StateGroup& sg)
{
    if (_first_stage_group)
    {  // skip default state grop(the first in the tree) TODO::change to detect default state
        _first_stage_group = false;
    }
    else
    {
        vsg::StateGroup::StateCommands& sc = sg.stateCommands;
        for (auto& state : sc)
        {
            auto bds = state.cast<vsg::BindDescriptorSet>();
            if (bds)
            {
                apply(*bds);
            }
        }
    }
    sg.traverse(*this);
}
void RayTracingSceneDescriptorCreationVisitor::apply(vsg::BindDescriptorSet& bds)
{
    // TODO: every material that is not set should get a default material assigned
    std::set<int> set_textures;
    is_opaque.push_back(true);
    for (const auto& descriptor : bds.descriptorSet->descriptors)
    {
        if (descriptor->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)  // pbr material
        {
            auto d = descriptor.cast<vsg::DescriptorBuffer>();
            if (d->bufferInfoList[0]->data->dataSize() == sizeof(vsg::PbrMaterial))
            {
                // pbr material
                vsg::PbrMaterial vsg_mat;
                std::memcpy(&vsg_mat, d->bufferInfoList[0]->data->dataPointer(), sizeof(vsg::PbrMaterial));
                WaveFrontMaterialPacked mat;
                std::memcpy(&mat.ambient_roughness, &vsg_mat.baseColorFactor, sizeof(vsg::vec4));
                std::memcpy(&mat.specular_dissolve, &vsg_mat.specularFactor, sizeof(vsg::vec4));
                std::memcpy(&mat.diffuse_ior, &vsg_mat.diffuseFactor, sizeof(vsg::vec4));
                // std::memcpy(&mat.diffuseIor, &vsgMat.baseColorFactor, sizeof(vsg::vec4));
                std::memcpy(&mat.emission_texture_id, &vsg_mat.emissiveFactor, sizeof(vsg::vec4));
                std::memcpy(&mat.transmittance_illum, &vsg_mat.transmissionFactor, sizeof(vsg_mat.transmissionFactor));
                _mesh_emissive = vsg_mat.emissiveFactor.r + vsg_mat.emissiveFactor.g + vsg_mat.emissiveFactor.b != 0;
                mat.ambient_roughness.w = vsg_mat.roughnessFactor;
                mat.diffuse_ior.w = vsg_mat.indexOfRefraction;
                mat.specular_dissolve.w = vsg_mat.alphaMask;
                mat.emission_texture_id.w = vsg_mat.alphaMaskCutoff;
                mat.category_id = vsg_mat.categoryId;
                if (vsg_mat.transmissionFactor.x != 1 || vsg_mat.transmissionFactor.y != 1
                    || vsg_mat.transmissionFactor.z != 1)
                {
                    mat.transmittance_illum.w = 7;  // means that refraction and reflection should be active
                }
                _material_array.push_back(mat);
            }
            else
            {
                // normal material
                vsg::PhongMaterial vsg_mat;
                std::memcpy(&vsg_mat, d->bufferInfoList[0]->data->dataPointer(), sizeof(vsg::PhongMaterial));
                WaveFrontMaterialPacked mat{};
                std::memcpy(&mat.ambient_roughness, &vsg_mat.ambient, sizeof(vsg::vec4));
                std::memcpy(&mat.specular_dissolve, &vsg_mat.specular, sizeof(vsg::vec4));
                std::memcpy(&mat.diffuse_ior, &vsg_mat.diffuse, sizeof(vsg::vec4));
                std::memcpy(&mat.emission_texture_id, &vsg_mat.emissive, sizeof(vsg::vec4));
                std::memcpy(&mat.transmittance_illum, &vsg_mat.transmissive, sizeof(vsg_mat.transmissive));
                _mesh_emissive = vsg_mat.emissive.r + vsg_mat.emissive.g + vsg_mat.emissive.b != 0;
                // mapping of shininess to roughness: http://simonstechblog.blogspot.com/2011/12/microfacet-brdf.html
                auto shin2_rough = [](float shininess) { return std::sqrt(2 / (shininess + 2)); };
                mat.ambient_roughness.w = shin2_rough(vsg_mat.shininess);
                mat.diffuse_ior.w = vsg_mat.indexOfRefraction;
                mat.specular_dissolve.w = vsg_mat.alphaMask;
                mat.emission_texture_id.w = vsg_mat.alphaMaskCutoff;
                mat.category_id = vsg_mat.categoryId;
                if (vsg_mat.transmissive.x != 1 || vsg_mat.transmissive.y != 1 || vsg_mat.transmissive.z != 1)
                {
                    mat.transmittance_illum.w = 7;  // means that refraction and reflection should be active
                }
                _material_array.push_back(mat);
            }
            continue;
        }

        if (descriptor->descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {
            continue;
        }

        vsg::ref_ptr<vsg::DescriptorImage> d = descriptor.cast<vsg::DescriptorImage>();  // cast to descriptor image
        vsg::ref_ptr<vsg::DescriptorImage> texture;
        switch (descriptor->dstBinding)
        {
        case 0:  // diffuse map
            texture = vsg::DescriptorImage::create(d->imageInfoList, 6, _diffuse.size());
            _diffuse.push_back(texture);
            set_textures.insert(6);
            // check for opaqueness
            {
                auto data = d->imageInfoList[0]->imageView->image->data;
                // int amt = data->dataSize() / data->stride();
                for (int i = 0; i < data->dataSize() / data->stride() && is_opaque.back(); ++i)
                {
                    void* d = static_cast<char*>(data->dataPointer()) + i * data->stride();
                    switch (data->getLayout().format)
                    {
                    case VK_FORMAT_R32G32B32A32_SFLOAT:
                        if (static_cast<float*>(d)[3] < .01F)
                        {
                            is_opaque.back() = false;
                        }
                        break;
                    case VK_FORMAT_R8G8B8A8_UNORM:
                    case VK_FORMAT_B8G8R8A8_UNORM:
                        if (static_cast<uint8_t*>(d)[3] < .01F * 255)
                        {
                            is_opaque.back() = false;
                        }
                        break;
                    }
                }
            }
            break;
        case 1:  // metall roughness map
            texture = vsg::DescriptorImage::create(d->imageInfoList, 7, _mr.size());
            _mr.push_back(texture);
            set_textures.insert(7);
            break;
        case 2:  // normal map
            texture = vsg::DescriptorImage::create(d->imageInfoList, 8, _normal.size());
            _normal.push_back(texture);
            set_textures.insert(8);
            break;
        case 3:  // light map
            break;
        case 4:  // emissive map
            texture = vsg::DescriptorImage::create(d->imageInfoList, 10, _emissive.size());
            _emissive.push_back(texture);
            set_textures.insert(10);
            break;
        case 5:  // specular map
            texture = vsg::DescriptorImage::create(d->imageInfoList, 11, _specular.size());
            _specular.push_back(texture);
            set_textures.insert(11);
            break;
        default:
            std::cout << "Unkown texture binding: " << descriptor->dstBinding << ". Could not properly detect material"
                      << std::endl;
        }
    }

    // setting the default texture for not set textures
    for (int i = 6; i < 12; ++i)
    {
        if (set_textures.find(i) == set_textures.end())
        {
            vsg::ref_ptr<vsg::DescriptorImage> texture;
            switch (i)
            {
            case 6:
                texture = vsg::DescriptorImage::create(_default_texture->imageInfoList, i, _diffuse.size());
                _diffuse.push_back(texture);
                break;
            case 7:
                texture = vsg::DescriptorImage::create(_default_texture->imageInfoList, i, _mr.size());
                _mr.push_back(texture);
                break;
            case 8:
                texture = vsg::DescriptorImage::create(_default_texture->imageInfoList, i, _normal.size());
                _normal.push_back(texture);
                break;
            case 9:
                break;
            case 10:
                texture = vsg::DescriptorImage::create(_default_texture->imageInfoList, i, _emissive.size());
                _emissive.push_back(texture);
                break;
            case 11:
                texture = vsg::DescriptorImage::create(_default_texture->imageInfoList, i, _specular.size());
                _specular.push_back(texture);
                break;
            }
        }
    }
}
void RayTracingSceneDescriptorCreationVisitor::apply(const vsg::Light& l)
{
    packed_lights.emplace_back(l);
}
void RayTracingSceneDescriptorCreationVisitor::update_descriptor(
    vsg::BindDescriptorSet* desc_set, const vsg::BindingMap& binding_map)
{
    if (packed_lights.empty())
    {
        std::cout << "Adding default directional light for raytracing" << std::endl;
        auto l = vsg::DirectionalLight::create();
        vsg::vec3 col{2.F, 2.F, 2.F};
        l->color = col;
        l->direction = normalize(vsg::vec3(0.1F, 1, -5.1F));
        l->strengths = vsg::vec3(.5F, .0F, .0F);
        packed_lights.emplace_back(*l);
    }
    if (!_lights)
    {
        float strength_sum = 0;
        for (auto& light : packed_lights)
        {
            // TODO: complete
            // strength_sum += light.colorAmbient.x + light.colorAmbient.y + light.colorAmbient.z + light.colorDiffuse.x
            //                + light.colorDiffuse.y + light.colorDiffuse.z + light.colorSpecular.x
            //                + light.colorSpecular.y + light.colorSpecular.z;
            // light.inclusiveStrength = strength_sum;
        }
        auto lights = vsg::Array<PackedLight>::create(packed_lights.size());
        std::copy(packed_lights.begin(), packed_lights.end(), lights->data());
        _lights = vsg::DescriptorBuffer::create(lights, 12, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }
    if (!_materials)
    {
        auto materials = vsg::Array<WaveFrontMaterialPacked>::create(_material_array.size());
        std::copy(_material_array.begin(), _material_array.end(), materials->data());
        _materials = vsg::DescriptorBuffer::create(materials, 13, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }
    if (!_instances)
    {
        auto instances = vsg::Array<ObjectInstance>::create(_instances_array.size());
        std::copy(_instances_array.begin(), _instances_array.end(), instances->data());
        _instances = vsg::DescriptorBuffer::create(instances, 14, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }

    // setting the descriptor amount for the object arrays
    vsg::DescriptorSetLayoutBindings& bindings = desc_set->descriptorSet->setLayout->bindings;
    int pos_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "Pos").second;
    std::find_if(
        bindings.begin(), bindings.end(), [pos_ind](VkDescriptorSetLayoutBinding& b) { return b.binding == pos_ind; })
        ->descriptorCount
        = static_cast<uint32_t>(_positions.size());
    int nor_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "Nor").second;
    std::find_if(
        bindings.begin(), bindings.end(), [nor_ind](VkDescriptorSetLayoutBinding& b) { return b.binding == nor_ind; })
        ->descriptorCount
        = static_cast<uint32_t>(_normals.size());
    int tex_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "Tex").second;
    std::find_if(
        bindings.begin(), bindings.end(), [tex_ind](VkDescriptorSetLayoutBinding& b) { return b.binding == tex_ind; })
        ->descriptorCount
        = static_cast<uint32_t>(_tex_coords.size());
    int ind_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "Ind").second;
    std::find_if(
        bindings.begin(), bindings.end(), [ind_ind](VkDescriptorSetLayoutBinding& b) { return b.binding == ind_ind; })
        ->descriptorCount
        = static_cast<uint32_t>(_indices.size());
    int diffuse_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "diffuseMap").second;
    std::find_if(bindings.begin(), bindings.end(),
        [diffuse_ind](VkDescriptorSetLayoutBinding& b) { return b.binding == diffuse_ind; })
        ->descriptorCount
        = static_cast<uint32_t>(_diffuse.size());
    int mr_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "mrMap").second;
    std::find_if(
        bindings.begin(), bindings.end(), [mr_ind](VkDescriptorSetLayoutBinding& b) { return b.binding == mr_ind; })
        ->descriptorCount
        = static_cast<uint32_t>(_mr.size());
    int normal_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "normalMap").second;
    std::find_if(bindings.begin(), bindings.end(),
        [normal_ind](VkDescriptorSetLayoutBinding& b) { return b.binding == normal_ind; })
        ->descriptorCount
        = static_cast<uint32_t>(_normal.size());
    int emissive_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "emissiveMap").second;
    std::find_if(bindings.begin(), bindings.end(),
        [emissive_ind](VkDescriptorSetLayoutBinding& b) { return b.binding == emissive_ind; })
        ->descriptorCount
        = static_cast<uint32_t>(_emissive.size());
    int specular_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "specularMap").second;
    std::find_if(bindings.begin(), bindings.end(),
        [specular_ind](VkDescriptorSetLayoutBinding& b) { return b.binding == specular_ind; })
        ->descriptorCount
        = static_cast<uint32_t>(_specular.size());
    int light_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "Lights").second;
    int mat_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "Materials").second;
    int instances_ind = vsg::ShaderStage::getSetBindingIndex(binding_map, "Instances").second;

    // adding all descriptors and updating their binding
    vsg::Descriptors desc_list;
    for (auto& d : _diffuse)
    {
        d->dstBinding = diffuse_ind;
        desc_list.push_back(d);
    }
    for (auto& d : _mr)
    {
        d->dstBinding = mr_ind;
        desc_list.push_back(d);
    }
    for (auto& d : _normal)
    {
        d->dstBinding = normal_ind;
        desc_list.push_back(d);
    }
    for (auto& d : _emissive)
    {
        d->dstBinding = emissive_ind;
        desc_list.push_back(d);
    }
    for (auto& d : _specular)
    {
        d->dstBinding = specular_ind;
        desc_list.push_back(d);
    }
    _lights->dstBinding = light_ind;
    _materials->dstBinding = mat_ind;
    _instances->dstBinding = instances_ind;
    desc_list.push_back(_lights);
    desc_list.push_back(_materials);
    desc_list.push_back(_instances);
    for (auto& d : _positions)
    {
        d->dstBinding = pos_ind;
        desc_list.push_back(d);
    }
    for (auto& d : _normals)
    {
        d->dstBinding = nor_ind;
        desc_list.push_back(d);
    }
    for (auto& d : _tex_coords)
    {
        d->dstBinding = tex_ind;
        desc_list.push_back(d);
    }
    for (auto& d : _indices)
    {
        d->dstBinding = ind_ind;
        desc_list.push_back(d);
    }
    desc_set->descriptorSet->descriptors = desc_list;
}

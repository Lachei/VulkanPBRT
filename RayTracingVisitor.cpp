#include "RayTracingVisitor.hpp"

RayTracingSceneDescriptorCreationVisitor::RayTracingSceneDescriptorCreationVisitor()
{
    vsg::ubvec4 w{ 255, 255, 255, 255 };
    auto white = vsg::ubvec4Array2D::create(1, 1, vsg::Data::Layout{ VK_FORMAT_R8G8B8A8_UNORM });
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
}
void RayTracingSceneDescriptorCreationVisitor::apply(vsg::Object& object)
{
    object.traverse(*this);
}
void RayTracingSceneDescriptorCreationVisitor::apply(vsg::MatrixTransform& mt)
{
    _transformStack.pushAndPreMult(mt.getMatrix());

    mt.traverse(*this);

    _transformStack.pop();
}
void RayTracingSceneDescriptorCreationVisitor::apply(vsg::VertexIndexDraw& vid)
{
    if (vid.arrays.empty()) return;

    //check cache
    bool cached = _vertexIndexDrawMap.find(&vid) != _vertexIndexDrawMap.end();
    ObjectInstance instance;
    instance.objectMat = _transformStack.top();
    if (cached)
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
        std::vector<vsg::vec3> nors(1);
        std::memcpy(nors.data(), vid.arrays[1]->dataPointer(), sizeof(vsg::vec3));
        if (vsg::length2(nors[0]) == 0)
        {
            //normals have to be computed
            nors.resize(vid.arrays[1]->dataSize() / sizeof(nors[0]));
            std::memcpy(nors.data(), vid.arrays[1]->dataPointer(), vid.arrays[1]->dataSize());
            std::vector<float> weightSum(nors.size(), 0);
            std::vector<vsg::vec3> positions(vid.arrays[0]->dataSize() / sizeof(vsg::vec3));
            std::memcpy(positions.data(), vid.arrays[0]->dataPointer(), vid.arrays[0]->dataSize());
            if (vid.indices->stride() == 4)
            {
                //integer indices
                std::vector<uint32_t> indices(vid.indices->dataSize() / sizeof(uint32_t));
                std::memcpy(indices.data(), vid.indices->dataPointer(), vid.indices->dataSize());
                for (int tri = 0; tri < indices.size() / 3; ++tri)
                {
                    uint32_t indA = indices[3 * tri], indB = indices[3 * tri + 1], indC = indices[3 * tri + 2];
                    vsg::vec3 &a = positions[indA], &b = positions[indB], &c = positions[indC];
                    vsg::vec3 faceNormal = vsg::cross(b - a, c - a);
                    float w = vsg::length(faceNormal); //the weight is proportional to the area of the face
                    faceNormal /= w;
                    float newW = w + weightSum[indA];
                    nors[indA] = nors[indA] * (weightSum[indA] / newW) + faceNormal * (w / newW);
                    weightSum[indA] = newW;
                    newW = w + weightSum[indB];
                    nors[indB] = nors[indB] * (weightSum[indB] / newW) + faceNormal * (w / newW);
                    weightSum[indB] = newW;
                    newW = w + weightSum[indC];
                    nors[indC] = nors[indC] * (weightSum[indC] / newW) + faceNormal * (w / newW);
                    weightSum[indC] = newW;
                }
            }
            else
            {
                //short indices
                std::vector<uint16_t> indices(vid.indices->dataSize() / sizeof(uint16_t));
                std::memcpy(indices.data(), vid.indices->dataPointer(), vid.indices->dataSize());
                for (int tri = 0; tri < indices.size() / 3; ++tri)
                {
                    uint32_t indA = indices[3 * tri], indB = indices[3 * tri + 1], indC = indices[3 * tri + 2];
                    vsg::vec3 &a = positions[indA], &b = positions[indB], &c = positions[indC];
                    vsg::vec3 faceNormal = vsg::cross(b - a, c - a);
                    float w = vsg::length(faceNormal); //the weight is proportional to the area of the face
                    faceNormal /= w;
                    float newW = w + weightSum[indA];
                    nors[indA] = nors[indA] * (weightSum[indA] / newW) + faceNormal * (w / newW);
                    weightSum[indA] = newW;
                    newW = w + weightSum[indB];
                    nors[indB] = nors[indB] * (weightSum[indB] / newW) + faceNormal * (w / newW);
                    weightSum[indB] = newW;
                    newW = w + weightSum[indC];
                    nors[indC] = nors[indC] * (weightSum[indC] / newW) + faceNormal * (w / newW);
                    weightSum[indC] = newW;
                }
            }
            //for(auto& normal: nors) normal = vsg::normalize(normal);
            std::memcpy(vid.arrays[1]->dataPointer(), nors.data(), vid.arrays[1]->dataSize());
        }
        auto normals = vsg::DescriptorBuffer::create(vid.arrays[1], 3, _normals.size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        _normals.push_back(normals);
        // auto fill up tex coords if not provided
        vsg::ref_ptr<vsg::DescriptorBuffer> texCoords;
        int v = vid.arrays[2]->valueCount();
        if (vid.arrays[2]->valueCount() == 0)
        {
            auto data = vsg::vec2Array::create(vid.arrays[0]->valueCount());
            for (int i = 0; i < vid.indices->valueCount() / 3; ++i)
            {
                uint32_t index = 0;
                if (vid.indices->stride() == 2) index = ((uint16_t*)(vid.indices->dataPointer()))[i * 3];
                else index = ((uint32_t*)(vid.indices->dataPointer()))[i * 3];
                data->at(index) = vsg::vec2(0, 0);
                if (vid.indices->stride() == 2) index = ((uint16_t*)(vid.indices->dataPointer()))[i * 3 + 1];
                else index = ((uint32_t*)(vid.indices->dataPointer()))[i * 3 + 1];
                data->at(index) = vsg::vec2(0, 1);
                if (vid.indices->stride() == 2) index = ((uint16_t*)(vid.indices->dataPointer()))[i * 3 + 2];
                else index = ((uint32_t*)(vid.indices->dataPointer()))[i * 3 + 2];
                data->at(index) = vsg::vec2(1, 0);
            }
            vid.arrays[2] = data;
        }
        else texCoords = vsg::DescriptorBuffer::create(vid.arrays[2], 4, _texCoords.size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        _texCoords.push_back(texCoords);
        auto indices = vsg::DescriptorBuffer::create(vid.indices, 5, _indices.size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        _indices.push_back(indices);
        instance.indexStride = vid.indices->stride();
    }
    _instancesArray.push_back(instance);

    //if emissive mesh create a mesh light for each triangle
    if (meshEmissive)
    {
        for (int i = 0; i < vid.indices->valueCount() / 3; ++i)
        {
            vsg::Light l{};
            l.radius = 0;
            l.type = vsg::LightSourceType::Area;
            l.colorAmbient = {
                _materialArray.back().emissionTextureId.r, _materialArray.back().emissionTextureId.g,
                _materialArray.back().emissionTextureId.b
            };
            l.colorDiffuse = l.colorAmbient;
            l.colorSpecular = l.colorAmbient;
            l.strengths = vsg::vec3(0, 0, 1);

            uint32_t index = 0;
            if (vid.indices->stride() == 2) index = ((uint16_t*)(vid.indices->dataPointer()))[i * 3];
            else index = ((uint32_t*)(vid.indices->dataPointer()))[i * 3];
            l.v0 = ((vsg::vec3*)vid.arrays[0]->dataPointer())[index];
            if (vid.indices->stride() == 2) index = ((uint16_t*)(vid.indices->dataPointer()))[i * 3 + 1];
            else index = ((uint32_t*)(vid.indices->dataPointer()))[i * 3 + 1];
            l.v1 = ((vsg::vec3*)vid.arrays[0]->dataPointer())[index];
            if (vid.indices->stride() == 2) index = ((uint16_t*)(vid.indices->dataPointer()))[i * 3 + 2];
            else index = ((uint32_t*)(vid.indices->dataPointer()))[i * 3 + 2];
            l.v2 = ((vsg::vec3*)vid.arrays[0]->dataPointer())[index];
            packedLights.push_back(l.getPacked());
        }
    }
}
void RayTracingSceneDescriptorCreationVisitor::apply(vsg::StateGroup& sg)
{
    if (firstStageGroup) //skip default state grop(the first in the tree) TODO::change to detect default state
        firstStageGroup = false;
    else
    {
        vsg::StateGroup::StateCommands& sc = sg.getStateCommands();
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
    std::set<int> setTextures;
    isOpaque.push_back(true);
    for (const auto& descriptor : bds.descriptorSet->descriptors)
    {
        if (descriptor->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) //pbr material
        {
            auto d = descriptor.cast<vsg::DescriptorBuffer>();
            if (d->bufferInfoList[0].data->dataSize() == sizeof(VsgPbrMaterial))
            {
                // pbr material
                VsgPbrMaterial vsgMat;
                std::memcpy(&vsgMat, d->bufferInfoList[0].data->dataPointer(), sizeof(VsgPbrMaterial));
                WaveFrontMaterialPacked mat;
                std::memcpy(&mat.ambientShininess, &vsgMat.baseColorFactor, sizeof(vsg::vec4));
                std::memcpy(&mat.specularDissolve, &vsgMat.specularFactor, sizeof(vsg::vec4));
                std::memcpy(&mat.diffuseIor, &vsgMat.diffuseFactor, sizeof(vsg::vec4));
                std::memcpy(&mat.emissionTextureId, &vsgMat.emissiveFactor, sizeof(vsg::vec4));
                if (vsgMat.emissiveFactor.r + vsgMat.emissiveFactor.g + vsgMat.emissiveFactor.b != 0) meshEmissive = true;
                else meshEmissive = false;
                mat.ambientShininess.w = vsgMat.roughnessFactor;
                mat.diffuseIor.w = vsgMat.indexOfRefraction;
                mat.specularDissolve.w = vsgMat.alphaMask;
                mat.emissionTextureId.w = vsgMat.alphaMaskCutoff;
                _materialArray.push_back(mat);
            }
            else
            {
                // normal material
                VsgMaterial vsgMat;
                std::memcpy(&vsgMat, d->bufferInfoList[0].data->dataPointer(), sizeof(VsgMaterial));
                WaveFrontMaterialPacked mat;
                std::memcpy(&mat.ambientShininess, &vsgMat.ambient, sizeof(vsg::vec4));
                std::memcpy(&mat.specularDissolve, &vsgMat.specular, sizeof(vsg::vec4));
                std::memcpy(&mat.diffuseIor, &vsgMat.diffuse, sizeof(vsg::vec4));
                std::memcpy(&mat.emissionTextureId, &vsgMat.emissive, sizeof(vsg::vec4));
                if (vsgMat.emissive.r + vsgMat.emissive.g + vsgMat.emissive.b != 0) meshEmissive = true;
                else meshEmissive = false;
                mat.ambientShininess.w = vsgMat.shininess;
                mat.diffuseIor.w = 1;
                mat.specularDissolve.w = vsgMat.alphaMask;
                mat.emissionTextureId.w = vsgMat.alphaMaskCutoff;
                _materialArray.push_back(mat);
            }
            continue;
        }

        if (descriptor->descriptorType != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) continue;

        vsg::ref_ptr<vsg::DescriptorImage> d = descriptor.cast<vsg::DescriptorImage>(); //cast to descriptor image
        vsg::ref_ptr<vsg::DescriptorImage> texture;
        switch (descriptor->dstBinding)
        {
        case 0: //diffuse map
            texture = vsg::DescriptorImage::create(d->imageInfoList, 6, _diffuse.size());
            _diffuse.push_back(texture);
            setTextures.insert(6);
            // check for opaqueness
            {
                auto data = d->imageInfoList[0].imageView->image->data;
                //int amt = data->dataSize() / data->stride();
                for (int i = 0; i < data->dataSize() / data->stride() && isOpaque.back(); ++i)
                {
                    void* d = static_cast<char*>(data->dataPointer()) + i * data->stride();
                    switch (data->getLayout().format)
                    {
                    case VK_FORMAT_R32G32B32A32_SFLOAT:
                        if (static_cast<float*>(d)[3] < .01f) isOpaque.back() = false;
                        break;
                    case VK_FORMAT_R8G8B8A8_UNORM:
                    case VK_FORMAT_B8G8R8A8_UNORM:
                        if (static_cast<uint8_t*>(d)[3] < .01f * 255) isOpaque.back() = false;
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
    for (int i = 6; i < 12; ++i)
    {
        if (setTextures.find(i) == setTextures.end())
        {
            vsg::ref_ptr<vsg::DescriptorImage> texture;
            switch (i)
            {
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
void RayTracingSceneDescriptorCreationVisitor::apply(const vsg::Light& l)
{
    packedLights.push_back(l.getPacked());
}
void RayTracingSceneDescriptorCreationVisitor::updateDescriptor(vsg::BindDescriptorSet* descSet, const vsg::BindingMap& bindingMap)    
{
    if (packedLights.empty())
    {
        std::cout << "Adding default directional light for raytracing" << std::endl;
        vsg::Light l;
        l.radius = 0;
        l.type = vsg::LightSourceType::Directional;
        vsg::vec3 col{2.f, 2.f, 2.f};
        l.colorAmbient = col;
        l.colorDiffuse = col;
        l.colorSpecular = {0, 0, 0};
        l.strengths = vsg::vec3(.5f, .0f, .0f);
        l.dir = vsg::normalize(vsg::vec3(0.1f, 1, -5.1f));
        packedLights.push_back(l.getPacked());
    }
    if (!_lights)
    {
        auto lights = vsg::Array<vsg::Light::PackedLight>::create(packedLights.size());
        std::copy(packedLights.begin(), packedLights.end(), lights->data());
        _lights = vsg::DescriptorBuffer::create(lights, 12, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }
    if (!_materials)
    {
        auto materials = vsg::Array<WaveFrontMaterialPacked>::create(_materialArray.size());
        std::copy(_materialArray.begin(), _materialArray.end(), materials->data());
        _materials = vsg::DescriptorBuffer::create(materials, 13, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }
    if (!_instances)
    {
        auto instances = vsg::Array<ObjectInstance>::create(_instancesArray.size());
        std::copy(_instancesArray.begin(), _instancesArray.end(), instances->data());
        _instances = vsg::DescriptorBuffer::create(instances, 14, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }

    // setting the descriptor amount for the object arrays
    vsg::DescriptorSetLayoutBindings& bindings = descSet->descriptorSet->setLayout->bindings;
    int posInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Pos").second;
    std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b) { return b.binding == posInd; })->
        descriptorCount = static_cast<uint32_t>(_positions.size());
    int norInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Nor").second;
    std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b) { return b.binding == norInd; })->
        descriptorCount = static_cast<uint32_t>(_normals.size());
    int texInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Tex").second;
    std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b) { return b.binding == texInd; })->
        descriptorCount = static_cast<uint32_t>(_texCoords.size());
    int indInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Ind").second;
    std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b) { return b.binding == indInd; })->
        descriptorCount = static_cast<uint32_t>(_indices.size());
    int diffuseInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "diffuseMap").second;
    std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b) { return b.binding == diffuseInd; })->
        descriptorCount = static_cast<uint32_t>(_diffuse.size());
    int mrInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "mrMap").second;
    std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b) { return b.binding == mrInd; })->descriptorCount
        = static_cast<uint32_t>(_mr.size());
    int normalInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "normalMap").second;
    std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b) { return b.binding == normalInd; })->
        descriptorCount = static_cast<uint32_t>(_normal.size());
    int emissiveInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "emissiveMap").second;
    std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b) { return b.binding == emissiveInd; })->
        descriptorCount = static_cast<uint32_t>(_emissive.size());
    int specularInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "specularMap").second;
    std::find_if(bindings.begin(), bindings.end(), [&](VkDescriptorSetLayoutBinding& b) { return b.binding == specularInd; })->
        descriptorCount = static_cast<uint32_t>(_specular.size());
    int lightInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Lights").second;
    int matInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Materials").second;
    int instancesInd = vsg::ShaderStage::getSetBindingIndex(bindingMap, "Instances").second;

    //adding all descriptors and updating their binding
    vsg::Descriptors descList;
    for (auto& d : _diffuse)
    {
        d->dstBinding = diffuseInd;
        descList.push_back(d);
    }
    for (auto& d : _mr)
    {
        d->dstBinding = mrInd;
        descList.push_back(d);
    }
    for (auto& d : _normal)
    {
        d->dstBinding = normalInd;
        descList.push_back(d);
    }
    for (auto& d : _emissive)
    {
        d->dstBinding = emissiveInd;
        descList.push_back(d);
    }
    for (auto& d : _specular)
    {
        d->dstBinding = specularInd;
        descList.push_back(d);
    }
    _lights->dstBinding = lightInd;
    _materials->dstBinding = matInd;
    _instances->dstBinding = instancesInd;
    descList.push_back(_lights);
    descList.push_back(_materials);
    descList.push_back(_instances);
    for (auto& d : _positions)
    {
        d->dstBinding = posInd;
        descList.push_back(d);
    }
    for (auto& d : _normals)
    {
        d->dstBinding = norInd;
        descList.push_back(d);
    }
    for (auto& d : _texCoords)
    {
        d->dstBinding = texInd;
        descList.push_back(d);
    }
    for (auto& d : _indices)
    {
        d->dstBinding = indInd;
        descList.push_back(d);
    }
    descSet->descriptorSet->descriptors = descList;
}

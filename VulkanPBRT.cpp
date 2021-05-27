 #include <iostream>
 #include <set>
 #include <vsg/all.h>
 #include <vsgXchange/models.h>
 #include <vsgXchange/images.h>
 
 #include "gui.hpp"

 struct RayTracingPushConstants{
     vsg::mat4 viewInverse;
     vsg::mat4 projInverse;
 };

class RayTracingPushConstantsValue : public vsg::Inherit<vsg::Value<RayTracingPushConstants>, RayTracingPushConstantsValue>{
    public:
    RayTracingPushConstantsValue(){}
};

class CountTrianglesVisitor : public vsg::Visitor
{
public:
    CountTrianglesVisitor():triangleCount(0){};

    void apply(vsg::Object& object){
        object.traverse(*this);
    };
    void apply(vsg::Geometry& geometry){
        triangleCount += geometry.indices->valueCount() / 3;
    };

    void apply(vsg::VertexIndexDraw& vid)
    {
        triangleCount += vid.indices->valueCount() / 3;
    }

    int triangleCount;
};

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
    CreateRayTracingDescriptorTraversal(vsg::ref_ptr<vsg::PipelineLayout> pipelineLayout): pipelineLayout(pipelineLayout){};

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

    //getting the texture samplers of the descriptor sets
    void apply(vsg::BindDescriptorSet &bds){
        // TODO: every material that is not set should get a default material assigned
        std::set<int> setTextures;
        for(const auto& descriptor: bds.descriptorSet->descriptors)
        {
            if(descriptor->descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) //pbr material
            {
                vsg::ref_ptr<vsg::DescriptorBuffer> d = descriptor.cast<vsg::DescriptorBuffer>();
                VsgPbrMaterial vsgMat;
                std::memcpy(&vsgMat, d->bufferInfoList[0].data->dataPointer(), sizeof(VsgPbrMaterial));
                WaveFrontMaterialPacked mat;
                std::memcpy(&mat.ambientShininess, &vsgMat.baseColorFactor, sizeof(vsg::vec3));
                std::memcpy(&mat.specularDissolve, &vsgMat.specularFactor, sizeof(vsg::vec3));
                std::memcpy(&mat.diffuseIor, &vsgMat.diffuseFactor, sizeof(vsg::vec3));
                mat.ambientShininess.w = vsgMat.roughnessFactor;
                mat.diffuseIor.w = 1;
                mat.specularDissolve.w = vsgMat.alphaMask;
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
                texture = vsg::DescriptorImage::create(d->imageInfoList, 9, _ao.size());
                _ao.push_back(texture);
                setTextures.insert(9);
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
                    texture = vsg::DescriptorImage::create(_defaultTexture->imageInfoList, i, _ao.size());
                    _ao.push_back(texture);
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
        _packedLights.push_back(l.getPacked());
    }

    vsg::ref_ptr<vsg::BindDescriptorSet> getBindDescriptorSet(){
        if(!_bindDescriptor){
            if(_packedLights.empty())
            {
                std::cout << "Adding default directional light for raytracing" << std::endl;
                vsg::Light l;
                l.radius = 0;
                l.type = vsg::LightSourceType::Directional;
                vsg::vec3 col{1.0f,1.0f,1.0f};
                l.colorAmbient = col;
                l.colorDiffuse = col;
                l.colorSpecular = col;
                l.strengths = vsg::vec3(1.0f,.0f,.0f);
                l.dir = vsg::vec3(0, .0, -1.0f);
                _packedLights.push_back(l.getPacked());
            }
            if(!_lights)
            {
                auto lights = vsg::Array<vsg::Light::PackedLight>::create(_packedLights.size());
                std::copy(_packedLights.begin(), _packedLights.end(), lights->data());
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
            vsg::DescriptorSetLayoutBindings descriptorBindings{
                {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
                {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},
                {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(_positions.size()), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
                {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(_normals.size()), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
                {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(_indices.size()), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
                {5, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, static_cast<uint32_t>(_indices.size()), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
                {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(_diffuse.size()), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
                {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(_mr.size()), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
                {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(_normal.size()), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
                {9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(_ao.size()), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
                {10, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(_emissive.size()), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
                {11, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(_specular.size()), VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
                {12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
                {13, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr},
                {14, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, nullptr}
                };
            //adding all descriptors
            vsg::Descriptors descList;
            for(const auto& d: _diffuse){
                descList.push_back(d);
            }
            for(const auto& d: _mr){
                descList.push_back(d);
            }
            for(const auto& d: _normal){
                descList.push_back(d);
            }
            for(const auto& d: _ao){
                descList.push_back(d);
            }
            for(const auto& d: _emissive){
                descList.push_back(d);
            }
            for(const auto& d: _specular){
                descList.push_back(d);
            }
            descList.push_back(_lights);
            descList.push_back(_materials);
            descList.push_back(_instances);
            for(const auto& d: _positions){
                descList.push_back(d);
            }
            for(const auto& d: _normals){
                descList.push_back(d);
            }
            for(const auto& d: _texCoords){
                descList.push_back(d);
            }
            for(const auto& d: _indices){
                descList.push_back(d);
            }
            _descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);
            _descriptorSet = vsg::DescriptorSet::create(_descriptorSetLayout, descList);
            if(!pipelineLayout)
            {
                pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{_descriptorSetLayout}, vsg::PushConstantRanges{{VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(RayTracingPushConstants)}});
            }
            _bindDescriptor = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, _descriptorSet);
        }
        return _bindDescriptor;
    };

    //holds the binding command for the raytracing decriptor
    vsg::ref_ptr<vsg::PipelineLayout> pipelineLayout;
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
    std::vector<vsg::ref_ptr<vsg::DescriptorImage>> _ao;
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
    std::vector<vsg::Light::PackedLight> _packedLights;

    std::map<vsg::VertexIndexDraw*, ObjectInstance> _vertexIndexDrawMap;
    vsg::MatrixStack _transformStack;

    vsg::ref_ptr<vsg::DescriptorImage> _defaultTexture;   //the default image is used for each texture that is not available
    bool firstStageGroup = true;                        //the first state group contains the default state which should be skipped
};

int main(int argc, char** argv){
    try{
        // command line parsing
        vsg::CommandLine arguments(&argc, argv);

        auto windowTraits = vsg::WindowTraits::create();
        windowTraits->windowTitle = "VulkanPBRT";
        windowTraits->width = 1800;
        //windowTraits->height = 1080;
        windowTraits->debugLayer = true;//arguments.read({"--debug", "-d"});
        windowTraits->apiDumpLayer = arguments.read({"--api", "-a"});
        if(arguments.read({"--fullscreen", "-fs"})) windowTraits->fullscreen = true;
        if(arguments.read({"--window", "-w"}, windowTraits->width, windowTraits->height)) windowTraits->fullscreen = false;
        arguments.read("--screen", windowTraits->screenNum);

        auto numFrames = arguments.value(-1, "-f");
        auto filename = arguments.value(std::string(), "-i");
        filename = "/home/lachei/Downloads/glTF-Sample-Models-master/2.0/Sponza/glTF/Sponza.gltf";//"/home/lachei/Downloads/teapot.obj";
        if(arguments.read("m")) filename = "models/raytracing_scene.vsgt";
        if(arguments.errors()) return arguments.writeErrorMessages(std::cerr);

        //viewer creation
        auto viewer = vsg::Viewer::create();
        windowTraits->queueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
        windowTraits->imageAvailableSemaphoreWaitFlag = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        windowTraits->swapchainPreferences.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        windowTraits->deviceExtensionNames = {VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME
        , VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, VK_KHR_SPIRV_1_4_EXTENSION_NAME, VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME};
        windowTraits->vulkanVersion = VK_API_VERSION_1_2;
        auto& enabledAccelerationStructureFeatures = windowTraits->deviceFeatures->get<VkPhysicalDeviceAccelerationStructureFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR>();
        auto& enabledRayTracingPipelineFeatures = windowTraits->deviceFeatures->get<VkPhysicalDeviceRayTracingPipelineFeaturesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR>();
        enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;
        enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
        auto& enabledPhysicalDeviceVk12Feature = windowTraits->deviceFeatures->get<VkPhysicalDeviceVulkan12Features, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES>();
        enabledPhysicalDeviceVk12Feature.runtimeDescriptorArray = VK_TRUE;
        enabledPhysicalDeviceVk12Feature.bufferDeviceAddress = VK_TRUE;
        enabledPhysicalDeviceVk12Feature.descriptorIndexing = VK_TRUE;
        enabledPhysicalDeviceVk12Feature.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
        enabledPhysicalDeviceVk12Feature.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

        auto window = vsg::Window::create(windowTraits);
        if(!window){
            std::cout << "Could not create windows." << std::endl;
            return 1;
        }
        vsg::ref_ptr<vsg::Device> device;
        try{
            device = window->getOrCreateDevice();
        }
        catch(const vsg::Exception& e){
            std::cout << e.message << " Vk Result = " << e.result << std::endl;
            return 0;
        }
        //setting a custom render pass
        vsg::AttachmentDescription  colorAttachment = vsg::defaultColorAttachment(window->surfaceFormat().format);
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        //colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        vsg::AttachmentDescription depthAttachment = vsg::defaultDepthAttachment(window->depthFormat());
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        vsg::RenderPass::Attachments attachments{
            colorAttachment,
            depthAttachment};

        VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef = {};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        vsg::SubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachments.emplace_back(colorAttachmentRef);
        subpass.depthStencilAttachments.emplace_back(depthAttachmentRef);

        vsg::RenderPass::Subpasses subpasses{subpass};

        // image layout transition
        VkSubpassDependency colorDependency = {};
        colorDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        colorDependency.dstSubpass = 0;
        colorDependency.srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        colorDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        colorDependency.srcAccessMask = 0;
        colorDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        colorDependency.dependencyFlags = 0;

        // depth buffer is shared between swap chain images
        VkSubpassDependency depthDependency = {};
        depthDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        depthDependency.dstSubpass = 0;
        depthDependency.srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        depthDependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthDependency.dependencyFlags = 0;

        vsg::RenderPass::Dependencies dependencies{colorDependency, depthDependency};

        auto renderPass = vsg::RenderPass::create(device, attachments, subpasses, dependencies);
        window->setRenderPass(renderPass);
        //window->clearColor() = {};
        viewer->addWindow(window);

        //shader creation
        const uint32_t shaderIndexRaygen = 0;
        const uint32_t shaderIndexMiss = 1;
        const uint32_t shaderIndexClosestHit = 2;
        //std::string raygenPath = "shaders/simple_raygen.rgen.spv";
        //std::string raymissPath = "shaders/simple_miss.rmiss.spv";
        //std::string closesthitPath = "shaders/simple_closesthit.rchit.spv";
        std::string raygenPath = "shaders/raygen.rgen.spv";
        std::string raymissPath = "shaders/miss.rmiss.spv";
        std::string shadowMissPath = "shaders/shadow.rmiss.spv";
        std::string closesthitPath = "shaders/pbr_closesthit.rchit.spv";

        auto raygenShader = vsg::ShaderStage::read(VK_SHADER_STAGE_RAYGEN_BIT_KHR, "main", raygenPath);
        auto raymissShader = vsg::ShaderStage::read(VK_SHADER_STAGE_MISS_BIT_KHR, "main", raymissPath);
        auto shadowMissShader = vsg::ShaderStage::read(VK_SHADER_STAGE_MISS_BIT_KHR, "main", shadowMissPath);
        auto closesthitShader = vsg::ShaderStage::read(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "main", closesthitPath);
        if(!raygenShader || !raymissShader || !closesthitShader || !shadowMissShader){
            std::cout << "Shader creation failed" << std::endl;
            return 1;
        }

        auto shaderStage = vsg::ShaderStages{raygenShader, raymissShader, shadowMissShader, closesthitShader};

        auto raygenShaderGroup = vsg::RayTracingShaderGroup::create();
        raygenShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        raygenShaderGroup->generalShader = 0;
        auto raymissShaderGroup = vsg::RayTracingShaderGroup::create();
        raymissShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        raymissShaderGroup->generalShader = 1;
        auto shadowMissShaderGroup = vsg::RayTracingShaderGroup::create();
        shadowMissShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        shadowMissShaderGroup->generalShader = 2;
        auto closesthitShaderGroup = vsg::RayTracingShaderGroup::create();
        closesthitShaderGroup->type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        closesthitShaderGroup->closestHitShader = 3;
        auto shaderGroups = vsg::RayTracingShaderGroups{raygenShaderGroup, raymissShaderGroup, shadowMissShaderGroup, closesthitShaderGroup};

        auto shaderBindingTable = vsg::RayTracingShaderBindingTable::create();
        shaderBindingTable->bindingTableEntries.raygenGroups = {raygenShaderGroup};
        shaderBindingTable->bindingTableEntries.raymissGroups = {raymissShaderGroup, shadowMissShaderGroup};
        shaderBindingTable->bindingTableEntries.hitGroups = {closesthitShaderGroup};

        //creating camera things
        auto perspective = vsg::Perspective::create(60, static_cast<double>(windowTraits->width) / static_cast<double>(windowTraits->height), .1, 1000);
        vsg::ref_ptr<vsg::LookAt> lookAt;

        vsg::ref_ptr<vsg::TopLevelAccelerationStructure> tlas;
        vsg::ref_ptr<vsg::BindDescriptorSet> rayTracingBinder;
        vsg::ref_ptr<vsg::PipelineLayout> rayTracingPipelineLayout;
        auto guiValues = Gui::Values::create();
        guiValues->raysPerPixel = 2;
        guiValues->width = windowTraits->width;
        guiValues->height = windowTraits->height;
        if(filename.empty()){
            //no extern geometry
            //acceleration structures
            auto vertices = vsg::vec3Array::create(
                {{-1.0f, -1.0f, 0.0f},
                {1.0f, -1.0f, 0.0f},
                {0.0f, 1.0f, 0.0f}}
            );
            auto indices = vsg::uintArray::create({0,1,2});

            auto accelGeometry = vsg::AccelerationGeometry::create();
            accelGeometry->verts = vertices;
            accelGeometry->indices = indices;

            auto blas = vsg::BottomLevelAccelerationStructure::create(device);
            blas->geometries.push_back(accelGeometry);

            tlas = vsg::TopLevelAccelerationStructure::create(device);

            auto geominstance = vsg::GeometryInstance::create();
            geominstance->accelerationStructure = blas;
            geominstance->transform = vsg::mat4();

            tlas->geometryInstances.push_back(geominstance);

            lookAt = vsg::LookAt::create(vsg::dvec3(0,0,-2.5), vsg::dvec3(0,0,0), vsg::dvec3(0,1,0));
            guiValues->triangleCount = 1;
        }
        else{
            auto options = vsg::Options::create(vsgXchange::assimp::create(), vsgXchange::dds::create(), vsgXchange::stbi::create()); //using the assimp loader
            auto loaded_scene = vsg::read_cast<vsg::Node>(filename, options);
            if(!loaded_scene){
                std::cout << "Scene not found: " << filename << std::endl;
                return 1;
            }
            vsg::BuildAccelerationStructureTraversal buildAccelStruct(device);
            loaded_scene->accept(buildAccelStruct);
            tlas = buildAccelStruct.tlas;

            CreateRayTracingDescriptorTraversal buildDescriptorBinding;
            loaded_scene->accept(buildDescriptorBinding);
            rayTracingBinder = buildDescriptorBinding.getBindDescriptorSet();
            rayTracingPipelineLayout = buildDescriptorBinding.pipelineLayout;

            lookAt = vsg::LookAt::create(vsg::dvec3(0.0, 1.0, -5.0), vsg::dvec3(0.0, 0.5, 0.0), vsg::dvec3(0.0, 1.0, 0.0));
            CountTrianglesVisitor counter;
            loaded_scene->accept(counter);
            guiValues->triangleCount = counter.triangleCount;
        }

        vsg::CompileTraversal compile(window);
        //tlas->compile(compile.context);

        auto storageImage = vsg::Image::create();
        storageImage->imageType = VK_IMAGE_TYPE_2D;
        storageImage->format = VK_FORMAT_B8G8R8A8_UNORM;
        storageImage->extent.width = windowTraits->width;
        storageImage->extent.height = windowTraits->height;
        storageImage->extent.depth = 1;
        storageImage->mipLevels = 1;
        storageImage->arrayLayers = 1;
        storageImage->samples = VK_SAMPLE_COUNT_1_BIT;
        storageImage->tiling = VK_IMAGE_TILING_OPTIMAL;
        storageImage->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
        storageImage->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        storageImage->flags = 0;
        storageImage->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vsg::ImageInfo storageImageInfo{nullptr, vsg::createImageView(compile.context, storageImage, VK_IMAGE_ASPECT_COLOR_BIT), VK_IMAGE_LAYOUT_GENERAL};
        
        auto rayTracingPushConstantsValue = RayTracingPushConstantsValue::create();
        perspective->get_inverse(rayTracingPushConstantsValue->value().projInverse);
        lookAt->get_inverse(rayTracingPushConstantsValue->value().viewInverse);
        auto pushConstants = vsg::PushConstants::create(VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, rayTracingPushConstantsValue);

        //set up graphics pipeline
        vsg::DescriptorSetLayoutBindings descriptorBindings{
            {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr}, //acceleration structure
            {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},              //output image
            {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr},             //camear matrices
            {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, }
        };
        auto descriptorSetLayout = vsg::DescriptorSetLayout::create(descriptorBindings);

        auto accelDescriptor = vsg::DescriptorAccelerationStructure::create(vsg::AccelerationStructures{tlas}, 0, 0);
        rayTracingBinder->descriptorSet->descriptors.push_back(accelDescriptor);
        auto storageImageDescriptor = vsg::DescriptorImage::create(storageImageInfo,1, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        rayTracingBinder->descriptorSet->descriptors.push_back(storageImageDescriptor);
        //auto raytracingUniformDescriptor = vsg::DescriptorBuffer::create(raytracingUniform, 2, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        //auto raytracingUniformDescriptor = vsg::DescriptorBuffer::create(raytracingUniform, 15, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        //rayTracingBinder->descriptorSet->descriptors.push_back(raytracingUniformDescriptor);
        //raytracingUniformDescriptor->copyDataListToBuffers();

        auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout}, vsg::PushConstantRanges{});
        //auto raytracingPipeline = vsg::RayTracingPipeline::create(pipelineLayout, shaderStage, shaderGroups);
        auto raytracingPipeline = vsg::RayTracingPipeline::create(rayTracingPipelineLayout, shaderStage, shaderGroups, shaderBindingTable, 2);
        auto bindRayTracingPipeline = vsg::BindRayTracingPipeline::create(raytracingPipeline);

        //auto descriptorSet = vsg::DescriptorSet::create(descriptorSetLayout, vsg::Descriptors{accelDescriptor, storageImageDescriptor, raytracingUniformDescriptor});
        //auto bindDescriptorSets = vsg::BindDescriptorSets::create(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, raytracingPipeline->getPipelineLayout(), 0, vsg::DescriptorSets{descriptorSet});


        //state group to bind the pipeline and descriptorset
        auto scenegraph = vsg::Commands::create();
        scenegraph->addChild(bindRayTracingPipeline);
        scenegraph->addChild(pushConstants);
        //scenegraph->addChild(bindDescriptorSets);
        scenegraph->addChild(rayTracingBinder);

        //ray trace setup
        auto traceRays = vsg::TraceRays::create();
        traceRays->bindingTable = shaderBindingTable;
        traceRays->width = windowTraits->width;
        traceRays->height = windowTraits->height;
        traceRays->depth = 1;

        scenegraph->addChild(traceRays);

        auto viewport = vsg::ViewportState::create(0, 0, windowTraits->width, windowTraits->height);
        auto camera = vsg::Camera::create(perspective, lookAt, viewport);

        auto commandGraph = vsg::CommandGraph::create(window);
        auto renderGraph = vsg::createRenderGraphForView(window, camera, vsgImGui::RenderImGui::create(window, Gui(guiValues)));//vsg::RenderGraph::create(window); // render graph for gui rendering
        renderGraph->clearValues.clear();   //removing clear values to avoid clearing the raytraced image
        auto copyImageViewToWindow = vsg::CopyImageViewToWindow::create(storageImageInfo.imageView, window);

        commandGraph->addChild(scenegraph);
        commandGraph->addChild(copyImageViewToWindow);
        //renderGraph->addChild(vsgImGui::RenderImGui::create(window, Gui(guiValues)));
        commandGraph->addChild(renderGraph);
        
        //close handler to close and imgui handler to forward to imgui
        viewer->addEventHandler(vsgImGui::SendEventsToImGui::create());
        viewer->addEventHandler(vsg::CloseHandler::create(viewer));
        viewer->addEventHandler(vsg::Trackball::create(camera));
        viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
        viewer->compile();

        while(viewer->advanceToNextFrame() && (numFrames < 0 || (numFrames--) > 0)){
            viewer->handleEvents();
            //update camera matrix
            lookAt->get_inverse(rayTracingPushConstantsValue->value().viewInverse);
            //raytracingUniformDescriptor->copyDataListToBuffers();

            viewer->update();
            viewer->recordAndSubmit();
            viewer->present();
        }
    }
    catch (const vsg::Exception& e){
        std::cout << e.message << " VkResult = " << e.result << std::endl;
        return 0;
    }
    return 0;
}
/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/Options.h>
#include <vsg/io/read.h>
#include <vsg/state/ShaderStage.h>
#include <vsg/traversals/CompileTraversal.h>
#include <vsg/core/Exception.h>
#include <SPIRV-Reflect/spirv_reflect.h>

using namespace vsg;

ShaderStage::ShaderStage()
{
}

ShaderStage::ShaderStage(VkShaderStageFlagBits in_stage, const std::string& in_entryPointName, ref_ptr<ShaderModule> shaderModule) :
    stage(in_stage),
    module(shaderModule),
    entryPointName(in_entryPointName)
{
}

ShaderStage::ShaderStage(VkShaderStageFlagBits in_stage, const std::string& in_entryPointName, const std::string& source, ref_ptr<ShaderCompileSettings> hints) :
    stage(in_stage),
    module(ShaderModule::create(source, hints)),
    entryPointName(in_entryPointName)
{
}

ShaderStage::ShaderStage(VkShaderStageFlagBits in_stage, const std::string& in_entryPointName, const ShaderModule::SPIRV& code) :
    stage(in_stage),
    module(ShaderModule::create(code)),
    entryPointName(in_entryPointName)
{
}

ShaderStage::ShaderStage(VkShaderStageFlagBits in_stage, const std::string& in_entryPointName, const std::string& source, const ShaderModule::SPIRV& code) :
    stage(in_stage),
    module(ShaderModule::create(source, code)),
    entryPointName(in_entryPointName)
{
}

ShaderStage::~ShaderStage()
{
}

ref_ptr<ShaderStage> ShaderStage::read(VkShaderStageFlagBits stage, const std::string& entryPointName, const std::string& filename, ref_ptr<const Options> options)
{
    auto object = vsg::read(filename, options);
    if (!object) return {};

    auto st = object.cast<vsg::ShaderStage>();
    if (!st)
    {
        auto sm = object.cast<vsg::ShaderModule>();
        return ShaderStage::create_if(sm.valid(), stage, entryPointName, sm);
    }

    st->stage = stage;
    st->entryPointName = entryPointName;
    return st;
}

ref_ptr<ShaderStage> ShaderStage::read(VkShaderStageFlagBits stage, const std::string& entryPointName, std::istream& fin, ref_ptr<const Options> options)
{
    auto object = vsg::read(fin, options);
    if (!object) return {};

    auto st = object.cast<vsg::ShaderStage>();
    if (!st)
    {
        auto sm = object.cast<vsg::ShaderModule>();
        return ShaderStage::create_if(sm.valid(), stage, entryPointName, sm);
    }

    st->stage = stage;
    st->entryPointName = entryPointName;
    return st;
}

void ShaderStage::read(Input& input)
{
    Object::read(input);

    if (input.version_greater_equal(0, 1, 4))
    {
        input.readValue<int32_t>("stage", stage);
        input.read("entryPointName", entryPointName);
        input.read("module", module);

        specializationConstants.clear();
        uint32_t numValues = input.readValue<uint32_t>("NumSpecializationConstants");
        for (uint32_t i = 0; i < numValues; ++i)
        {
            uint32_t id = input.readValue<uint32_t>("id");
            input.read("data", specializationConstants[id]);
        }
    }
    else
    {
        input.readValue<int32_t>("Stage", stage);
        input.read("EntryPoint", entryPointName);
        input.readObject("ShaderModule", module);

        specializationConstants.clear();
        uint32_t numValues = input.readValue<uint32_t>("NumSpecializationConstants");
        for (uint32_t i = 0; i < numValues; ++i)
        {
            uint32_t id = input.readValue<uint32_t>("constantID");
            input.readObject("data", specializationConstants[id]);
        }
    }
}

void ShaderStage::write(Output& output) const
{
    Object::write(output);

    if (output.version_greater_equal(0, 1, 4))
    {
        output.writeValue<int32_t>("stage", stage);
        output.write("entryPointName", entryPointName);
        output.write("module", module);

        output.writeValue<uint32_t>("NumSpecializationConstants", specializationConstants.size());
        for (auto& [id, data] : specializationConstants)
        {
            output.writeValue<uint32_t>("id", id);
            output.write("data", data);
        }
    }
    else
    {
        output.writeValue<int32_t>("Stage", stage);
        output.write("EntryPoint", entryPointName);
        output.write("ShaderModule", module);

        output.writeValue<uint32_t>("NumSpecializationConstants", specializationConstants.size());
        for (auto& [id, data] : specializationConstants)
        {
            output.writeValue<uint32_t>("constantID", id);
            output.write("data", data);
        }
    }
}

void ShaderStage::apply(Context& context, VkPipelineShaderStageCreateInfo& stageInfo) const
{
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = stage;
    stageInfo.module = module->vk(context.deviceID);
    stageInfo.pName = entryPointName.c_str();

    if (specializationConstants.empty())
    {
        stageInfo.pSpecializationInfo = nullptr;
    }
    else
    {
        uint32_t packedDataSize = 0;
        for (auto& id_data : specializationConstants)
        {
            packedDataSize += static_cast<uint32_t>(id_data.second->dataSize());
        }

        // allocate temporary memory to pack the specialization map and data into.
        auto mapEntries = context.scratchMemory->allocate<VkSpecializationMapEntry>(specializationConstants.size());
        auto packedData = context.scratchMemory->allocate<uint8_t>(packedDataSize);
        uint32_t offset = 0;
        uint32_t i = 0;
        for (auto& [id, data] : specializationConstants)
        {
            mapEntries[i++] = VkSpecializationMapEntry{id, offset, data->dataSize()};
            std::memcpy(packedData + offset, static_cast<uint8_t*>(data->dataPointer()), data->dataSize());
            offset += static_cast<uint32_t>(data->dataSize());
        }

        auto specializationInfo = context.scratchMemory->allocate<VkSpecializationInfo>(1);

        stageInfo.pSpecializationInfo = specializationInfo;

        // assign the values from the ShaderStage into the specializationInfo
        specializationInfo->mapEntryCount = static_cast<uint32_t>(specializationConstants.size());
        specializationInfo->pMapEntries = mapEntries;
        specializationInfo->dataSize = packedDataSize;
        specializationInfo->pData = packedData;
    }
}

void ShaderStage::compile(Context& context)
{
    if (module)
    {
        module->compile(context);
    }
}

const vsg::BindingMap& ShaderStage::getDescriptorSetLayoutBindingsMap()
{
    if(!_reflected) _createReflectData();
    return _descriptorSetLayoutBindingsMap;
}

const vsg::PushConstantRanges& ShaderStage::getPushConstantRanges()
{
    if(!_reflected) _createReflectData();
    return _pushConstantRanges;
}

void ShaderStage::_createReflectData()
{
    bool requiresShaderCompiler = module && module->code.empty() && !module->source.empty();
    if(requiresShaderCompiler){
        auto shaderCompiler = ShaderCompiler::create();
        if(shaderCompiler) shaderCompiler->compile(ref_ptr<ShaderStage>(this));
    }

    // Create the reflect shader module.
    SpvReflectShaderModule spvModule;
    SpvReflectResult result = spvReflectCreateShaderModule(module->code.size() * sizeof(module->code[0]), module->code.data(), &spvModule);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        throw Exception{"Error: vsg::ShaderStage::_createReflectData(...) failed to create SpvReflectResult.", result};
    }

    // Get reflection information on the descriptor sets.
    uint32_t numDescriptorSets = 0;
    result = spvReflectEnumerateDescriptorSets(&spvModule, &numDescriptorSets, nullptr);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        throw Exception{"Error: vsg::ShaderStage::_createReflectData(...) failed to get descriptor sets reflection.", result};
    }
    std::vector<SpvReflectDescriptorSet*> descriptorSets(numDescriptorSets);
    result = spvReflectEnumerateDescriptorSets(&spvModule, &numDescriptorSets, descriptorSets.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        throw Exception{"Error: vsg::ShaderStage::_createReflectData(...) failed to get descriptor sets reflection.", result};
    }

    for (SpvReflectDescriptorSet* reflectDescriptorSet: descriptorSets) {
        uint32_t setIdx = reflectDescriptorSet->set;
        for (uint32_t bindingIdx = 0; bindingIdx < reflectDescriptorSet->binding_count; bindingIdx++) {
            VkDescriptorSetLayoutBinding descriptorInfo{};
            descriptorInfo.binding = reflectDescriptorSet->bindings[bindingIdx]->binding;
            descriptorInfo.descriptorType = VkDescriptorType(reflectDescriptorSet->bindings[bindingIdx]->descriptor_type);
            if (reflectDescriptorSet->bindings[bindingIdx]->type_description
                    && reflectDescriptorSet->bindings[bindingIdx]->type_description->type_name
                    && strlen(reflectDescriptorSet->bindings[bindingIdx]->type_description->type_name) > 0) {
                _descriptorSetLayoutBindingsMap[setIdx].names.push_back(reflectDescriptorSet->bindings[bindingIdx]->type_description->type_name);
            } else {
                _descriptorSetLayoutBindingsMap[setIdx].names.push_back(reflectDescriptorSet->bindings[bindingIdx]->name);
            }
            descriptorInfo.descriptorCount = reflectDescriptorSet->bindings[bindingIdx]->count;
            descriptorInfo.stageFlags = stage;
            //TODO: add additional infos for images
            //descriptorInfo.readOnly = true;
            //descriptorInfo.image = reflectDescriptorSet->bindings[bindingIdx]->image;
            _descriptorSetLayoutBindingsMap[setIdx].bindings.push_back(descriptorInfo);

            if (reflectDescriptorSet->bindings[bindingIdx]->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE
                    || reflectDescriptorSet->bindings[bindingIdx]->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
                    || reflectDescriptorSet->bindings[bindingIdx]->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER
                    || reflectDescriptorSet->bindings[bindingIdx]->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC) {
                //descriptorInfo.readOnly = reflectDescriptorSet->bindings[0]->type_description->decoration_flags & SPV_REFLECT_DECORATION_NON_WRITABLE;
            }
        }
    }

    // Get reflection information on the push constant blocks.
    uint32_t numPushConstantBlocks = 0;
    result = spvReflectEnumeratePushConstantBlocks(&spvModule, &numPushConstantBlocks, nullptr);
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        throw Exception{"Error: vsg::ShaderStage::_createReflectData(...) failed to get push constant block reflection.", result};
    }
    std::vector<SpvReflectBlockVariable*> pushConstantBlockVariables(numPushConstantBlocks);
    result = spvReflectEnumeratePushConstantBlocks(&spvModule, &numPushConstantBlocks, pushConstantBlockVariables.data());
    if (result != SPV_REFLECT_RESULT_SUCCESS) {
        throw Exception{"Error: vsg::ShaderStage::_createReflectData(...) failed to get push constant block reflection.", result};
    }

    _pushConstantRanges.resize(numPushConstantBlocks);
    for (uint32_t blockIdx = 0; blockIdx < numPushConstantBlocks; blockIdx++) {
        VkPushConstantRange& pushConstantRange = _pushConstantRanges.at(blockIdx);
        SpvReflectBlockVariable* pushConstantBlockVariable = pushConstantBlockVariables.at(blockIdx);
        pushConstantRange.stageFlags = stage;
        pushConstantRange.offset = pushConstantBlockVariable->absolute_offset;
        pushConstantRange.size = pushConstantBlockVariable->size;
    }

    spvReflectDestroyShaderModule(&spvModule);
    _reflected = true;
}

BindingMap ShaderStage::mergeBindingMaps(const std::vector<BindingMap>& maps)
{
    BindingMap result;
    for(const auto& map: maps){
        for(const auto& entry: map){
            auto& resBindings = result[entry.first].bindings;
            auto& resNames = result[entry.first].names;
            const auto& bindings = entry.second.bindings;
            const auto& names = entry.second.names;
            for(int i = 0; i < static_cast<int>(bindings.size()); ++i){
                // only add binding if not yet in the binding vector of the result
                if(std::find_if(resBindings.begin(), resBindings.end(), [&](VkDescriptorSetLayoutBinding& b){return b.binding == bindings[i].binding;}) == resBindings.end()){
                    resBindings.push_back(bindings[i]);
                    resNames.push_back(names[i]);
                }
                // if binding does exist, check if names are the same
                else{
                    int bindingIndex = std::find_if(resBindings.begin(), resBindings.end(), [&](VkDescriptorSetLayoutBinding& b){return b.binding == bindings[i].binding;}) - resBindings.begin();
                    int nameIndex = std::find_if(resNames.begin(), resNames.end(), [&](std::string& s){return s == names[i];}) - resNames.begin();
                    if(bindingIndex != nameIndex){
                        throw Exception{"Error: vsg::mergeBindingMaps(...) descriptor bindings are not mergable.", 0};
                    }
                    else{   //add stage mask
                        resBindings[bindingIndex].stageFlags |= bindings[i].stageFlags;
                    }
                }
            }
        }
    }
    return result;
}

SetBindingIndex ShaderStage::getSetBindingIndex(const BindingMap& map, const std::string_view& name)
{
    for(auto& entry: map){
        for(int i = 0; i < static_cast<int>(entry.second.names.size()); ++i){
            if(entry.second.names[i] == name){
                return {entry.first, entry.second.bindings[i].binding};
            }
        }
    }
    throw Exception{"Error: vsg::getSetBindingIndex(...) binding name not in Binding Map:" + std::string(name), 0};
}

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

#include<vsg/raytracing/RayTracingShaderBindingTable.h>
#include<vsg/vk/Context.h>
#include<vsg/vk/Extensions.h>
#include<vsg/core/Exception.h>

void vsg::RayTracingShaderBindingTable::compile(Context& context){
    Extensions* extensions = Extensions::Get(context.device, true);
    auto rayTracingProperties = context.device->getPhysicalDevice()->getProperties<VkPhysicalDeviceRayTracingPipelinePropertiesKHR, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR>();
    auto alignedSize = [](uint32_t value, uint32_t alignment)
    {
        return (value + alignment - 1) & ~(alignment - 1);
    };
    //const uint32_t handleSizeAligned = alignedSize(rayTracingProperties.shaderGroupHandleSize, rayTracingProperties.shaderGroupHandleAlignment);
    const uint32_t handleSizeAligned = alignedSize(rayTracingProperties.shaderGroupHandleSize, rayTracingProperties.shaderGroupBaseAlignment);
    const uint32_t sbtSize = handleSizeAligned * shaderGroups.size();

    //BufferInfo bindingTableBufferInfo = context.stagingMemoryBufferPools->reserveBuffer(sbtSize, 4, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    //auto bindingTableBuffer = bindingTableBufferInfo.buffer;
    //auto bindingTableMemory = bindingTableBuffer->getDeviceMemory(context.deviceID);
    ref_ptr<Buffer> bindingTableBuffer = createBufferAndMemory(context.device, sbtSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_SHARING_MODE_EXCLUSIVE, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    std::vector<uint8_t> shaderHandleStorage(sbtSize);
    extensions->vkGetRayTracingShaderGroupHandlesKHR(*context.device, pipeline, 0, static_cast<uint32_t>(shaderGroups.size()), sbtSize, shaderHandleStorage.data());

    std::vector<int> bindingTableOffsets(4, 0);
    bindingTableOffsets[1] = bindingTableEntries.raygenGroups.size() * handleSizeAligned;
    bindingTableOffsets[2] = bindingTableOffsets[1] + bindingTableEntries.raymissGroups.size() * handleSizeAligned;
    bindingTableOffsets[3] = bindingTableOffsets[2] + bindingTableEntries.hitGroups.size() * handleSizeAligned;
    bindingTable.resize(4);
    for(auto& p: bindingTable) p = BufferInfo::create();
    bindingTable[0]->buffer = bindingTableBuffer;
    bindingTable[0]->offset = bindingTableOffsets[0];
    bindingTable[0]->range = bindingTableOffsets[1] - bindingTableOffsets[0];
    bindingTable[1]->buffer = bindingTableBuffer;
    bindingTable[1]->offset = bindingTableOffsets[1];
    bindingTable[1]->range = bindingTableOffsets[2] - bindingTableOffsets[1];
    bindingTable[2]->buffer = bindingTableBuffer;
    bindingTable[2]->offset = bindingTableOffsets[2];
    bindingTable[2]->range = bindingTableOffsets[3] - bindingTableOffsets[2];
    bindingTable[3]->buffer = bindingTableBuffer;
    bindingTable[3]->offset = bindingTableOffsets[3];
    bindingTable[3]->range = sbtSize - bindingTableOffsets[3];
    auto memory = bindingTableBuffer->getDeviceMemory(context.deviceID);
    void* d;
    memory->map(bindingTableBuffer->getMemoryOffset(context.device->deviceID), handleSizeAligned, 0, &d);
    uint8_t* data = reinterpret_cast<uint8_t*>(d);
    for (size_t i = 0; i < shaderGroups.size(); ++i)
    {
        auto cur = shaderGroups[i];
        int entry = -1;
        if(std::find(bindingTableEntries.raygenGroups.begin(), bindingTableEntries.raygenGroups.end(), cur) != bindingTableEntries.raygenGroups.end())
        {
            entry = 0;
        }
        else if(std::find(bindingTableEntries.raymissGroups.begin(), bindingTableEntries.raymissGroups.end(), cur) != bindingTableEntries.raymissGroups.end())
        {
            entry = 1;
        }
        else if(std::find(bindingTableEntries.hitGroups.begin(), bindingTableEntries.hitGroups.end(), cur) != bindingTableEntries.hitGroups.end())
        {
            entry = 2;
        }
        else if(std::find(bindingTableEntries.callableGroups.begin(), bindingTableEntries.callableGroups.end(), cur) != bindingTableEntries.callableGroups.end())
        {
            entry = 3;
        }
        if(entry < 0)
        {
            throw Exception{"Error: vsg::ShaderBindingTable::compile() ... missing binding table entry for shader group at index", (int)i};
        }
        memcpy(data + bindingTableOffsets[entry], shaderHandleStorage.data() + i * rayTracingProperties.shaderGroupHandleSize, handleSizeAligned);
        bindingTableOffsets[entry] += handleSizeAligned;
    }
    memory->unmap();
}
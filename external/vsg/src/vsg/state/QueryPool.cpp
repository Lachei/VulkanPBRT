#include <vsg/state/QueryPool.h>
#include <vsg/core/Exception.h>

using namespace vsg;

//////////////////////////////////////
//
// Query Pool
//

QueryPool::~QueryPool(){
    if(_queryPool)
        vkDestroyQueryPool(*_device, _queryPool, nullptr);
}

void QueryPool::read(Input& input){
    Object::read(input);

    input.readValue<uint32_t>("Flags", flags);
    input.readValue<uint32_t>("QueryType", queryType);
    input.readValue<uint32_t>("QueryCount", queryCount);
    input.readValue<uint32_t>("PipelineStatisticFlags", pipelineStatistics);
}

void QueryPool::write(Output& output) const{
    Object::write(output);

    output.writeValue<uint32_t>("Flags", flags);
    output.writeValue<uint32_t>("QueryType", queryType);
    output.writeValue<uint32_t>("QueryCount", queryCount);
    output.writeValue<uint32_t>("PipelineStatisticFlags", pipelineStatistics);
}

void QueryPool::reset(){
    if(_queryPool)
        vkResetQueryPool(*_device, _queryPool, 0, queryCount);
}

std::vector<uint32_t> QueryPool::getResults(){
    std::vector<uint32_t> res(queryCount);
    if (VkResult result = vkGetQueryPoolResults(*_device, _queryPool, 0, queryCount, queryCount * sizeof(uint32_t), res.data(), sizeof(uint32_t), VK_QUERY_RESULT_WAIT_BIT); result != VK_SUCCESS){
        throw Exception{"Error: Failed to get QueryPool results.", result};
    }
    return res;
}

void QueryPool::compile(Context& context){
    if(_queryPool) return;
    _device = context.device;
    VkQueryPoolCreateInfo createInfo {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, 
        {},         //pNext
        flags,      //flags
        queryType,  //queryType
        queryCount, //queryCount
        pipelineStatistics};
    if (VkResult res = vkCreateQueryPool(*context.device, &createInfo, nullptr, &_queryPool); res != VK_SUCCESS){
        throw Exception{"Error: Failed to create QueryPool.", res};
    }
}

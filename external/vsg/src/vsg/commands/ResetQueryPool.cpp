#include <vsg/commands/ResetQueryPool.h>

using namespace vsg;

void ResetQueryPool::compile(Context& context){
    queryPool->compile(context);
}

void ResetQueryPool::record(CommandBuffer& commandBuffer) const{
    vkCmdResetQueryPool(commandBuffer, *queryPool, 0, queryPool->queryCount);
}

#include <vsg/commands/WriteTimestamp.h>

using namespace vsg;

void WriteTimestamp::compile(Context& context){
    queryPool->compile(context);
}

void WriteTimestamp::record(CommandBuffer& commandBuffer) const{
    vkCmdWriteTimestamp(commandBuffer, pipelineStage, *queryPool, queryIndex);
}

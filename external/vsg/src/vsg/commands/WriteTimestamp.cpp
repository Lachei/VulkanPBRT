#include <vsg/commands/WriteTimestamp.h>

using namespace vsg;

void WriteTimestamp::read(Input& input){

}

void WriteTimestamp::write(Output& output) const{
    
}

void WriteTimestamp::compile(Context& context){
    queryPool->compile(context);
}

void WriteTimestamp::record(CommandBuffer& commandBuffer) const{
    vkCmdWriteTimestamp(commandBuffer, pipelineStage, *queryPool, queryIndex);
}

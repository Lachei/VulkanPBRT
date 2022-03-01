#pragma once

#include <vsg/commands/Command.h>
#include <vsg/state/QueryPool.h>

namespace vsg
{
    class VSG_DECLSPEC WriteTimestamp : public Inherit<Command, WriteTimestamp>
    {
    public:
        WriteTimestamp(ref_ptr<QueryPool> pool, uint32_t index, VkPipelineStageFlagBits stage): queryPool(pool), queryIndex(index), pipelineStage(stage) {};

        ref_ptr<QueryPool> queryPool;
        uint32_t queryIndex;
        VkPipelineStageFlagBits pipelineStage;

        void read(Input& input) override;
        void write(Output& output) const override;

        void compile(Context& context) override;

        void record(CommandBuffer& commandBuffer) const override;
    };
    VSG_type_name(vsg::WriteTimestamp);

} // namespace vsg
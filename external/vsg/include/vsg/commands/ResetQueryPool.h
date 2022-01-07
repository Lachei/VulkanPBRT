#pragma once

#include <vsg/commands/Command.h>
#include <vsg/state/QueryPool.h>

namespace vsg
{
    class VSG_DECLSPEC ResetQueryPool : public Inherit<Command, ResetQueryPool>
    {
    public:
        ResetQueryPool(ref_ptr<QueryPool> pool):queryPool(pool) {};

        ref_ptr<QueryPool> queryPool;

        void compile(Context& context) override;

        void record(CommandBuffer& commandBuffer) const override;
    };
    VSG_type_name(vsg::ResetQueryPool);

} // namespace vsg
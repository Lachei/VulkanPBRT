#pragma once

#include <vsg/core/Object.h>
#include <vsg/vk/Context.h>
#include <vsg/vk/Device.h>

namespace vsg
{
    class VSG_DECLSPEC QueryPool : public Inherit<Object, QueryPool>
    {
    public:
        QueryPool(){};

        ~QueryPool();

        VkQueryPoolCreateFlags flags = 0;
        VkQueryType queryType = VK_QUERY_TYPE_TIMESTAMP;      
        uint32_t queryCount = 1;
        VkQueryPipelineStatisticFlags pipelineStatistics = 0;

        void read(Input& input) override;
        void write(Output& output) const override;

        void reset();
        std::vector<uint32_t> getResults();

        void compile(Context& context);

        operator VkQueryPool() const { return _queryPool; }
    protected:
        VkQueryPool _queryPool{};
        ref_ptr<Device> _device{};
    };

} // namespace vsg

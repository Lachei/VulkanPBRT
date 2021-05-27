#include <vsg/vk/PipelineCache.h>

vsg::PipelineCache::PipelineCache(ref_ptr<Device> device,const std::string& filename)
:_device(device)
{

}

vsg::PipelineCache::~PipelineCache(){
    for(auto& cache: pipelineCaches)
    {
        vkDestroyPipelineCache(*_device, cache, nullptr);
    }
}
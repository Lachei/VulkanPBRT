#include "ResourceManager.h"

#include <utility>

using namespace vkpbrt;

ResourceManager::ResourceManager() : _next_resource_id(0) {}
ResourceManager::~ResourceManager()
{
    for (auto& id_resource : _resource_map)
    {
        delete id_resource.second;
        id_resource.second = nullptr;
    }
}
uint64_t ResourceManager::add_resource(ResourceBase* resource)
{
    uint64_t resource_id = _next_resource_id++;
    _resource_map[resource_id] = resource;
    return resource_id;
}
ResourceBase* ResourceManager::get_resource(uint64_t resource_id)
{
    auto iterator = _resource_map.find(resource_id);
    if (iterator == _resource_map.end())
    {
        return nullptr;
    }
    ResourceBase* resource = iterator->second;
    if (!resource->is_loaded())
    {
        resource->load();
    }
    return resource;
}

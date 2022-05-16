#pragma once
#include "ResourceBase.h"

#include <unordered_map>
#include <atomic>
#include <filesystem>

namespace vkpbrt
{
class ResourceManager
{
public:
    explicit ResourceManager(std::filesystem::path resource_directory_path);
    ~ResourceManager();

    uint64_t add_resource(ResourceBase* resource);
    ResourceBase* get_resource(uint64_t resource_id);

private:
    std::filesystem::path _resource_directory_path;
    std::unordered_map<uint64_t, ResourceBase*> _resource_map;
    std::atomic<uint64_t> _next_resource_id;
};
}  // namespace vkpbrt

#pragma once
#include <cstdint>

namespace vkpbrt
{
class EntityManager
{
public:
    template<typename T, typename... Args>
    void add_components(uint64_t entity_id);
    template<typename T>
    T* get_component(uint64_t entity_id);
};

template<typename T, typename... Args>
void EntityManager::add_components(uint64_t entity_id)
{
    // TODO:
}
template<typename T>
T* EntityManager::get_component(uint64_t entity_id)
{
    // TODO:
    return nullptr;
}
}  // namespace vkpbrt

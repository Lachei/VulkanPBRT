#pragma once
#include <cstdint>

namespace vkpbrt
{
class EntityManager
{
public:
    uint64_t create_entity();

    template<typename T>
    void add_component(uint64_t entity_id);
    template<typename T>
    T* get_component(uint64_t entity_id);
};

template<typename T>
void EntityManager::add_component(uint64_t entity_id)
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

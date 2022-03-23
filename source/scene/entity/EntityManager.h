#pragma once
#include <atomic>
#include <cstdint>

#include "ComponentRegistry.h"
#include "Archetype.h"

namespace vkpbrt
{
class EntityManager
{
public:
    EntityManager();

    uint64_t create_entity();

    template<typename ComponentType>
    void add_component(uint64_t entity_id);
    template<typename ComponentType>
    ComponentType* get_component(uint64_t entity_id);

private:
    void change_archetype(uint64_t entity_id, TypeMask target_component_type_mask);

    std::unordered_map<uint64_t, TypeMask> _entity_id_to_component_type_mask_map;
    std::unordered_map<TypeMask, Archetype> _component_type_mask_to_archetype_map;

    std::atomic<uint64_t> _next_entity_id;
};

template<typename ComponentType>
void EntityManager::add_component(uint64_t entity_id)
{
    TypeMask added_type_mask = type_mask<ComponentType>();
    TypeMask target_component_type_mask = _entity_id_to_component_type_mask_map[entity_id] | added_type_mask;
    change_archetype(entity_id, target_component_type_mask);
}
template<typename ComponentType>
ComponentType* EntityManager::get_component(uint64_t entity_id)
{
    TypeMask queried_component_type_mask = type_mask<ComponentType>();
    TypeMask current_component_type_mask = _entity_id_to_component_type_mask_map[entity_id];
    if (queried_component_type_mask & current_component_type_mask == 0)
    {
        return nullptr;
    }
    Archetype& archetype = _component_type_mask_to_archetype_map[current_component_type_mask];
    return archetype.get_component<ComponentType>(entity_id);
}
}  // namespace vkpbrt

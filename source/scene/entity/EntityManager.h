#pragma once
#include "ComponentRegistry.h"
#include "Archetype.h"
#include "Common.h"

#include <atomic>
#include <cstdint>

namespace vkpbrt
{
class EntityManager
{
public:
    EntityManager();

    EntityId create_entity();

    template<typename ComponentType>
    void add_component(EntityId entity_id);
    template<typename ComponentType>
    ComponentType* get_component(EntityId entity_id);

private:
    void change_archetype(EntityId entity_id, TypeMask target_component_type_mask);

    std::unordered_map<EntityId, TypeMask> _entity_id_to_component_type_mask_map;
    std::unordered_map<TypeMask, Archetype> _component_type_mask_to_archetype_map;

    std::atomic<EntityId> _next_entity_id;
};

template<typename ComponentType>
void EntityManager::add_component(EntityId entity_id)
{
    TypeMask added_type_mask = type_mask<ComponentType>();
    TypeMask target_component_type_mask = _entity_id_to_component_type_mask_map[entity_id] | added_type_mask;
    change_archetype(entity_id, target_component_type_mask);
}
template<typename ComponentType>
ComponentType* EntityManager::get_component(EntityId entity_id)
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

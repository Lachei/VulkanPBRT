#include "EntityManager.h"

using namespace vkpbrt;

EntityManager::EntityManager() : _next_entity_id(0) {}
uint64_t EntityManager::create_entity()
{
    TypeMask component_type_mask = 0;
    uint64_t entitiy_id = _next_entity_id++;
    _entity_id_to_component_type_mask_map[entitiy_id] = component_type_mask;
    return entitiy_id;
}
void EntityManager::change_archetype(uint64_t entity_id, TypeMask target_component_type_mask)
{
    TypeMask current_type_mask = _entity_id_to_component_type_mask_map[entity_id];
    if (current_type_mask != target_component_type_mask)
    {
        Archetype& current_archetype = _component_type_mask_to_archetype_map[current_type_mask];
        if (target_component_type_mask == 0)
        {
            // entity will have no components
            current_archetype.remove_entity(entity_id);
        }
        else
        {
            if (_component_type_mask_to_archetype_map.find(target_component_type_mask)
                == _component_type_mask_to_archetype_map.end())
            {
                _component_type_mask_to_archetype_map.emplace(
                    target_component_type_mask, Archetype(target_component_type_mask));
            }
            Archetype& target_archetype = _component_type_mask_to_archetype_map[target_component_type_mask];
            current_archetype.move_entity(entity_id, target_archetype);
        }

        _entity_id_to_component_type_mask_map[entity_id] = target_component_type_mask;
    }
}

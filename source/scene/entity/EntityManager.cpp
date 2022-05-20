#include "EntityManager.h"

using namespace vkpbrt;

std::atomic<EntityId> EntityManager::next_entity_id = 0;

EntityId EntityManager::create_entity()
{
    TypeMask component_type_mask = 0;
    EntityId entitiy_id = next_entity_id++;
    _entity_id_to_component_type_mask_map[entitiy_id] = component_type_mask;
    return entitiy_id;
}
void EntityManager::change_archetype(EntityId entity_id, TypeMask target_component_type_mask)
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
    else
    {
        // TODO: add a warning that a component type that already exists tried to be added
    }
}

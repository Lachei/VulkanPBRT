#include "Archetype.h"
#include <memory>
#include <cstdlib>

using namespace vkpbrt;

#define INITIAL_ENTITY_PER_ARCHETYPE_ALLOCATION 512

#ifdef WIN32
#define ALIGNED_ALLOC _aligned_malloc
#define ALIGNED_REALLOC(memblock, size, alignment) _aligned_realloc(memblock, size, alignment)
#else
#define ALIGNED_ALLOC aligned_alloc
#define ALIGNED_REALLOC(memblock, size, alignment) realloc(memblock, size)
#endif

Archetype::Archetype(TypeMask component_type_mask) : _entity_count(0)
{
    // extract individual bits from the type mask
    while (component_type_mask != 0)
    {
        TypeMask current_type_mask = component_type_mask & -component_type_mask;
        component_type_mask &= ~current_type_mask;
        const TypeInfo& type_info = ComponentRegistry::get_type_info(current_type_mask);

        // allocate aligned buffer
        ComponentBuffer& component_buffer = _type_mask_to_component_buffers_map[current_type_mask];
        component_buffer.size_in_bytes = type_info.size_in_bytes * INITIAL_ENTITY_PER_ARCHETYPE_ALLOCATION;
        component_buffer.buffer = static_cast<uint8_t*>(ALIGNED_ALLOC(
            type_info.size_in_bytes * INITIAL_ENTITY_PER_ARCHETYPE_ALLOCATION, type_info.alignment_in_bytes));
        component_buffer.type_info = type_info;
    }
    _entity_ids.resize(INITIAL_ENTITY_PER_ARCHETYPE_ALLOCATION);
}
void Archetype::add_entity(EntityId entitiy_id)
{
    uint32_t component_index = _entity_count++;
    _entity_id_component_index_map[entitiy_id] = component_index;
    bool resized = false;
    for (auto& type_mask_component_buffer : _type_mask_to_component_buffers_map)
    {
        ComponentBuffer& component_buffer = type_mask_component_buffer.second;
        const TypeInfo& type_info = component_buffer.type_info;
        size_t entity_offset_in_bytes = type_info.size_in_bytes * component_index;
        if (entity_offset_in_bytes + type_info.size_in_bytes > component_buffer.size_in_bytes)
        {
            // reallocate to a larger buffer
            component_buffer.size_in_bytes *= 2;
            component_buffer.buffer = static_cast<uint8_t*>(
                ALIGNED_REALLOC(component_buffer.buffer, component_buffer.size_in_bytes, type_info.alignment_in_bytes));
            resized = true;
        }
    }
    if (resized)
    {
        _entity_ids.resize(_entity_ids.size() * 2);
    }
    _entity_ids[component_index] = entitiy_id;
}
void Archetype::remove_entity(EntityId entity_id)
{
    // move the last components in the component buffers to where the components of the removed entity were stored
    // to ensure continuity in memory
    uint32_t last_component_index = --_entity_count;
    uint32_t removed_component_index = _entity_id_component_index_map[entity_id];
    for (auto& type_mask_component_buffer : _type_mask_to_component_buffers_map)
    {
        ComponentBuffer& component_buffer = type_mask_component_buffer.second;
        size_t component_size = component_buffer.type_info.size_in_bytes;
        uint8_t* source_address = component_buffer.buffer + component_size * last_component_index;
        uint8_t* target_address = component_buffer.buffer + component_size * removed_component_index;
        memcpy(target_address, source_address, component_size);
    }
    EntityId moved_entity_id = _entity_ids[last_component_index];
    _entity_ids[removed_component_index] = moved_entity_id;
    _entity_id_component_index_map[moved_entity_id] = removed_component_index;
    _entity_id_component_index_map.erase(entity_id);
}
void Archetype::move_entity(EntityId entity_id, Archetype& target_archetype)
{
    target_archetype.add_entity(entity_id);
    uint32_t source_component_index = _entity_id_component_index_map[entity_id];
    uint32_t target_component_index = target_archetype.get_entity_count();

    for (auto& target_buffer_iterator : target_archetype._type_mask_to_component_buffers_map)
    {
        size_t component_size = target_buffer_iterator.second.type_info.size_in_bytes;
        uint8_t* target_address = target_buffer_iterator.second.buffer + component_size * target_component_index;
        TypeMask target_type_mask = target_buffer_iterator.first;
        auto source_buffer_iterator = this->_type_mask_to_component_buffers_map.find(target_type_mask);
        if (source_buffer_iterator != this->_type_mask_to_component_buffers_map.end())
        {
            // copy components whose type are contained in both archetypes
            uint8_t* source_address = source_buffer_iterator->second.buffer + component_size * source_component_index;
            memcpy(target_address, source_address, component_size);
        }
        else
        {
            // call the component constructor when the component type did not exist in the source archetype
            target_buffer_iterator.second.type_info.constructor(target_address);
        }
    }
    remove_entity(entity_id);
}

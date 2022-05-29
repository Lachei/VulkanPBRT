#pragma once
#include "ComponentRegistry.h"
#include "Common.h"

#include <unordered_map>
#include <cstdint>
#include <cassert>

namespace vkpbrt
{
/*
 * All entites with the same set of components belong to the same Archetype.
 */
class Archetype
{
public:
    Archetype() = default;
    explicit Archetype(TypeMask component_type_mask);
    void add_entity(EntityId entitiy_id);
    void remove_entity(EntityId entity_id);
    void move_entity(EntityId entity_id, Archetype& target_archetype);

    template<typename ComponentType>
    ComponentType* get_component(EntityId entity_id);

    [[nodiscard]] uint32_t get_entity_count() const;

    template<typename ComponentType>
    ComponentType* get_component_array();

private:
    struct ComponentBuffer
    {
        uint8_t* buffer;
        size_t size_in_bytes;
        TypeInfo type_info;
    };
    std::unordered_map<TypeMask, ComponentBuffer> _type_mask_to_component_buffers_map;
    std::unordered_map<EntityId, uint32_t> _entity_id_component_index_map;
    std::vector<EntityId> _entity_ids;
    uint32_t _entity_count;
};

template<typename ComponentType>
ComponentType* Archetype::get_component(EntityId entity_id)
{
    uint32_t component_index = _entity_id_component_index_map[entity_id];
    assert(component_index < _entity_count);
    ComponentBuffer& component_buffer = _type_mask_to_component_buffers_map[type_mask<ComponentType>()];
    uint32_t component_offset = component_index * component_buffer.type_info.size_in_bytes;
    return reinterpret_cast<ComponentType*>(&component_buffer.buffer[component_offset]);
}
inline uint32_t Archetype::get_entity_count() const
{
    return _entity_count;
}
template<typename ComponentType>
ComponentType* Archetype::get_component_array()
{
    ComponentBuffer& component_buffer = _type_mask_to_component_buffers_map[type_mask<ComponentType>()];
    return reinterpret_cast<ComponentType*>(component_buffer.buffer);
}
}  // namespace vkpbrt

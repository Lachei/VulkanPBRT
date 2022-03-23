#pragma once
#include "ComponentRegistry.h"

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
    void add_entity(uint64_t entitiy_id);
    void move_entity(uint64_t entity_id, Archetype& target_archetype);
    void remove_entity(uint64_t entity_id);

    template<typename ComponentType>
    ComponentType* get_component(uint64_t entity_id);

    [[nodiscard]] uint32_t get_entity_count() const;

    template<typename ComponentType>
    ComponentType* get_component_array();

private:
    std::unordered_map<TypeMask, std::vector<uint8_t>> _component_buffers;
    std::unordered_map<uint64_t, uint32_t> _entity_id_component_index_map;
    uint32_t _entity_count;
};

template<typename ComponentType>
ComponentType* Archetype::get_component(uint64_t entity_id)
{
    uint32_t component_index = _entity_id_component_index_map[entity_id];
    assert(component_index < _entity_count);
    return reinterpret_cast<ComponentType*>(&_component_buffers[type_mask<ComponentType>()][component_index]);
}
inline uint32_t Archetype::get_entity_count() const
{
    return _entity_count;
}
template<typename ComponentType>
ComponentType* Archetype::get_component_array()
{
    return reinterpret_cast<ComponentType*>(_component_buffers[type_mask<ComponentType>()].data());
}
}  // namespace vkpbrt

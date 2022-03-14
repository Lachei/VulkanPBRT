#pragma once
#include "EntityManager.h"

#include <cstdint>

namespace vkpbrt
{
class Entity
{
public:
    Entity(uint64_t entity_id, EntityManager& entity_manager);
    /**
     * \brief Get a component that is attached to this entity.
     * \tparam T The type of the component which should be returned.
     * \return Pointer to the component. Can be nullptr if this entity does not have a component of type T.
     */
    template<typename T>
    T* get_component();
    template<typename T>
    void add_component();

private:
    uint64_t _id;
    EntityManager& _entity_manager;
};

inline Entity::Entity(uint64_t entity_id, EntityManager& entity_manager)
    : _id(entity_id), _entity_manager(entity_manager)
{
}
template<typename T>
T* Entity::get_component()
{
    return _entity_manager.get_component<T>(_id);
}
template<typename T>
void Entity::add_component()
{
    _entity_manager.add_component<T>(_id);
}
}  // namespace vkpbrt

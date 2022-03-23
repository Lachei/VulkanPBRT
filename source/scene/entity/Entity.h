#pragma once
#include "EntityManager.h"

#include <cstdint>

namespace vkpbrt
{
/**
 * \brief A handle to an entity.
 */
class Entity
{
public:
    Entity(uint64_t entity_id, EntityManager& entity_manager);

    /**
     * \brief Add a new component to this entity.
     * \tparam ComponentType The type of the component to be added.
     */
    template<typename ComponentType>
    void add_component();

    /**
     * \brief Get a component that is attached to this entity.
     * \tparam ComponentType The type of the component which should be returned.
     * \return Pointer to the component. Can be nullptr if this entity does not have a component of type T.
     */
    template<typename ComponentType>
    ComponentType* get_component();

private:
    uint64_t _id;
    EntityManager& _entity_manager;
};

inline Entity::Entity(uint64_t entity_id, EntityManager& entity_manager)
    : _id(entity_id), _entity_manager(entity_manager)
{
}
template<typename ComponentType>
void Entity::add_component()
{
    _entity_manager.add_component<ComponentType>(_id);
}
template<typename ComponentType>
ComponentType* Entity::get_component()
{
    return _entity_manager.get_component<ComponentType>(_id);
}

}  // namespace vkpbrt

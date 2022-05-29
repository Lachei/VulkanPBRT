#pragma once
#include "EntityManager.h"

#include "Common.h"
#include <cstdint>

namespace vkpbrt
{
/**
 * \brief A handle to an entity.
 */
class EntityHandle
{
public:
    EntityHandle(EntityId entity_id, EntityManager& entity_manager);

    /**
     * \brief Add a new default initialized component to this entity.
     * \tparam ComponentType The type of the component to be added.
     */
    template<typename ComponentType>
    void add_component();

    /**
     * \brief Add a new component to this entity.
     * \tparam ComponentType The type of the component to be added.
     */
    template<typename ComponentType>
    void add_component(ComponentType component);

    /**
     * \brief Get a component that is attached to this entity.
     * \tparam ComponentType The type of the component which should be returned.
     * \return Pointer to the component. Can be nullptr if this entity does not have a component of type T.
     */
    template<typename ComponentType>
    ComponentType* get_component();

private:
    EntityId _id;
    EntityManager& _entity_manager;
};

inline EntityHandle::EntityHandle(EntityId entity_id, EntityManager& entity_manager)
    : _id(entity_id), _entity_manager(entity_manager)
{
}
template<typename ComponentType>
void EntityHandle::add_component()
{
    _entity_manager.add_component<ComponentType>(_id);
}
template<typename ComponentType>
void EntityHandle::add_component(ComponentType component)
{
    _entity_manager.add_component<ComponentType>(_id);
    ComponentType* added_component = _entity_manager.get_component<ComponentType>();
    *added_component = component;
}
template<typename ComponentType>
ComponentType* EntityHandle::get_component()
{
    return _entity_manager.get_component<ComponentType>(_id);
}

}  // namespace vkpbrt

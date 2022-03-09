#pragma once
#include "EntityManager.h"

#include <cstdint>

namespace vkpbrt
{
class Entity
{
public:
    Entity(uint64_t entityId, EntityManager& entity_manager);
    /**
     * \brief Get a component that is attached to this entity.
     * \tparam T The type of the component which should be returned.
     * \return Pointer to the component. Can be nullptr if this entity does not have a component of type T.
     */
    template <typename T>
    T* GetComponent();
    template <typename T>
    void AddComponent();
private:
    uint64_t _id;
    EntityManager& _entity_manager;
};

inline Entity::Entity(uint64_t entityId, EntityManager& entity_manager)
    : _id(entityId), _entity_manager(entity_manager)
{
}
template <typename T>
T* Entity::GetComponent()
{
    return _entity_manager.GetComponent<T>(_id);
}
template <typename T>
void Entity::AddComponent()
{
    _entity_manager.AddComponents<T>(_id);
}
}

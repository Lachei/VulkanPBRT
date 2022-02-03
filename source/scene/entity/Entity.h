#pragma once
#include "EntityManager.h"

#include <cstdint>

class Entity
{
public:
    Entity(uint64_t entityId, EntityManager& entity_manager);
    uint64_t GetId() const;

    /**
     * \brief Get an component that is attached to this entity.
     * \tparam T The type of the component which should be returned.
     * \return Pointer to the component. Can be nullptr if this entity does not have a component of type T.
     */
    template <typename T>
    T* GetComponent();
    template <typename T, typename ... Args>
    void AddComponent();
private:
    uint64_t _id;
    EntityManager& _entity_manager;
};

inline Entity::Entity(uint64_t entityId, EntityManager& entity_manager)
    : _id(entityId), _entity_manager(entity_manager)
{
}
inline uint64_t Entity::GetId() const
{
    return _id;
}
template <typename T>
T* Entity::GetComponent()
{
    return _entity_manager.GetComponent<T>(_id);
}
template <typename T, typename ... Args>
void Entity::AddComponent()
{
    _entity_manager.AddComponents<T>(_id);
}

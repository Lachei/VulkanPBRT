#pragma once
#include "EntityManager.h"

#include <cstdint>

class Entity
{
public:
    Entity(uint64_t entityId, EntityManager& entity_manager);

    uint64_t GetId() const;

    template <typename T>
    T* const GetComponent();
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
inline uint64_t Entity::GetId() const
{
    return _id;
}
template <typename T>
T* const Entity::GetComponent()
{
    return _entity_manager->GetComponent<T>(_id);
}
template <typename T>
void Entity::AddComponent()
{
    _entity_manager->AddComponents<T>(_id);
}

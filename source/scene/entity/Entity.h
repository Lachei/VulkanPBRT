#pragma once
#include "EntityManager.h"

#include <cstdint>

class Entity
{
public:
    Entity(uint64_t entityId, EntityManager& scene);

    uint64_t GetId() const;

    template <typename T>
    T* const GetComponent();
    template <typename T>
    void AddComponent();
private:
    uint64_t _id;
    EntityManager& _scene;
};

inline Entity::Entity(uint64_t entityId, EntityManager& scene)
    : _id(entityId), _scene(scene)
{
}
inline uint64_t Entity::GetId() const
{
    return _id;
}
template <typename T>
T* const Entity::GetComponent()
{
    return _scene->GetComponent<T>(_id);
}
template <typename T>
void Entity::AddComponent()
{
    _scene->AddComponents<T>(_id);
}

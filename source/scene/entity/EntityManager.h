#pragma once
#include <cstdint>

class EntityManager
{
public:
    template <typename T, typename... Args>
    void AddComponents(uint64_t entity_id);
    template <typename T>
    T* GetComponent(uint64_t entity_id);
};

template <typename T, typename ... Args>
void EntityManager::AddComponents(uint64_t entity_id)
{
    // TODO:
}
template <typename T>
T* EntityManager::GetComponent(uint64_t entity_id)
{
    // TODO:
    return nullptr;
}

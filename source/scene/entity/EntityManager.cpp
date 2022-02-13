#include"EntityManager.h"
#include <stdexcept>

uint64_t EntityManager::CreateEntity()
{
    last_entity_id++;
    LookupTable.insert({ last_entity_id, NULL });
    return last_entity_id;
}

template <typename T>
void EntityManager::AddComponent(uint64_t entity_id, T&& Component)
{
    if (LookupTable.check(entity_id))
    {
        LookupTable[entity_id]->AddComponent(std::forward<T>(Component)); //pointer of ComponentsArray for this entity
    }
    else
    {
        throw std::invalid_argument("This Entity has not been created!");
    }
}

template <typename T>
void EntityManager::AddComponent(uint64_t entity_id, std::string& name, T& Component)
{
    if (LookupTable.check(entity_id))
    {
        LookupTable[entity_id]->AddComponent(name, std::forward<T>(Component)); //pointer of ComponentsArray for this entity
    }
    else
    {
        throw std::invalid_argument("This Entity has not been created!");
    }
}

template <typename T>//component type T
T* EntityManager::GetComponent(uint64_t entity_id)
{
    return LookupTable[entity_id]->GetComponent<T>();
}

Entity_TYPE EntityManager::GetEntityTYPE(uint64_t entity_id)
{
    return IDmapType.find(entity_id)->second;
}


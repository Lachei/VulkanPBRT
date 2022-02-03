#include"EntityManager.h"
#include <stdexcept>

template <typename Component>
uint64_t EntityManager::CreateEntity(Component comp)
{
    last_entity_id++;
    LookupTable.insert({ last_entity_id, comp });
    return last_entity_id;
}

template <typename Component>
void EntityManager::AddComponents(uint64_t entity_id, Component comp)
{
    if (LookupTable.check(entity_id))
    {
        LookupTable[entity_id]= comp;
    }
    else
    {
        throw std::invalid_argument("This Entity has not been created!");
    }
}
template <typename Component>
Component* EntityManager::GetComponent(uint64_t entity_id)
{
    return LookupTable[entity_id];
}

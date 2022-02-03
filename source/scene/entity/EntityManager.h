#pragma once
#include <cstdint>
#include"Entity.h"
#include <unordered_map>
class EntityManager
{
public:
    template <typename Component>
    void AddComponents(uint64_t entity_id, Component comp);
    template <typename Component>
    Component* GetComponent(uint64_t entity_id);
    template <typename Component>
    uint64_t CreateEntity(Component comp);
private:
    template <typename Component>
    std::unordered_map<uint64_t, Component> LookupTable;
    uint64_t last_entity_id=0;
};


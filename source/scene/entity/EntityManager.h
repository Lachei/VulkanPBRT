#pragma once
#include <cstdint>
#include"Entity.h"
#include <unordered_map>
#include"Components.h"
#include"ComponentsArray.h"
enum class Entity_TYPE{Camera, Light, Material};
class EntityManager
{
public:
    template <typename T>
    void AddComponent(uint64_t entity_id, T&& Component);
    template <typename T>
    T* GetComponent(uint64_t entity_id);// get component with defined component type for this entity id
    uint64_t EntityManager::CreateEntity();
    Entity_TYPE GetEntityTYPE(uint64_t entity_id);
//protected:
    //void AddKeyframes(uint64_t entity_id, std::string path);
private:
    //// Map from id  to this entity's component array pointer.
    std::unordered_map<uint64_t, std::shared_ptr<ComponentsArray>> LookupTable;//for each entity id, save all components for this entity
    std::unordered_map<uint64_t, Entity_TYPE> IDmapType;
    uint64_t last_entity_id=0;
    //// Map from type string pointer to a component type.
    //std::unordered_map<const char*, ComponentType> m_ComponentTypes{};
};


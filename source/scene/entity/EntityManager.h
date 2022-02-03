#pragma once
#include <cstdint>
#include"Entity.h"
#include <unordered_map>
#include<Components.h>

enum class Entity_TYPE{Camera, Light, Material};
class EntityManager
{
public:
    template <typename Component>
    void AddComponents(uint64_t entity_id, Component comp);
    template <typename Component>
    Component* GetComponent(uint64_t entity_id);
    template <typename Component>
    uint64_t CreateEntity(Component comp);
protected:
    void AddKeyframes(uint64_t entity_id, std::string path);
private:
    template <typename Component>
    std::unordered_map<uint64_t, Component> LookupTable;
    std::unordered_map<uint64_t, Entity_TYPE> IDmapType;
    uint64_t last_entity_id=0;
    Entity_TYPE GetEntityTYPE(uint64_t entity_id);
    template <typename T>
    std::vector<T> readKeyframes(std::string path);
    void AddKeyframesCamera(uint64_t entity_id, keyframeCameraData transforms);
    
};


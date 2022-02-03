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

template <typename T>
std::array<T> readKeyframes(std::string path)
{
    //TODO
    return NULL;
}

Entity_TYPE EntityManager::GetEntityTYPE(uint64_t entity_id)
{
    return IDmapType.find(entity_id)->second;
}

void EntityManager::AddKeyframesCamera(uint64_t entity_id, keyframeCameraData transforms)
{
    CameraComponent& cameracomp = GetComponent(entity_id);
    for (auto& t = transforms.begin(); t != transforms.end(); ++t)
        cameracomp.nextFrames_Transform.push_back(t);
}

template <typename T>//T could be float, vector3f,transform, Component struct...
void EntityManager::AddKeyframes(uint64_t entity_id, std::string path) 
{
    std::vector<T> KeyframesData = readKeyframes(path);
    if (GetEntityTYPE(entity_id) == Entity_TYPE::Camera)
    {
        AddKeyframesCamera(entity_id, KeyframesData);
    }
    //TODO
    //else if (...)
}
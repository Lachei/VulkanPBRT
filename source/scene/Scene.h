#pragma once
#include "entity/EntityHandle.h"
#include "entity/EntityManager.h"
#include "resources/ResourceManager.h"

#include <assimp/scene.h>

namespace vkpbrt
{
class Scene
{
public:
    explicit Scene(ResourceManager& resource_manager);
    Scene(ResourceManager& resource_manager, aiScene* ai_scene);

    EntityHandle create_entity();

    // TODO: add scene graph (parent child relations)

    // void get_render_data();  // TODO: return ray tracing descriptor
private:
    EntityManager _entity_manager;
    ResourceManager& _resource_manager;
};
}  // namespace vkpbrt

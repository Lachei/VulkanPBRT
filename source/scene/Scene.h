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

    EntityHandle create_entity();

    void load_from_assimp_scene(const aiScene* ai_scene);

    // TODO: add scene graph (parent child relations)

    // void get_render_data();  // TODO: return ray tracing descriptor
private:
    EntityManager _entity_manager;
    ResourceManager& _resource_manager;
};
}  // namespace vkpbrt

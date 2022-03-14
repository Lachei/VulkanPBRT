#include "Scene.h"

using namespace vkpbrt;

Entity Scene::create_entity()
{
    uint64_t entity_id = _entity_manager.create_entity();
    return Entity(entity_id, _entity_manager);
}

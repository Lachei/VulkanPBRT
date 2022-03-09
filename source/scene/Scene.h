#pragma once
#include "entity/Entity.h"
#include "entity/EntityManager.h"

namespace vkpbrt
{
class Scene
{
public:
    template<typename... InitialComponentTypes>
    Entity add_entity();

    void get_render_data(); // TODO: return ray tracing descriptor
private:
    EntityManager _entity_manager;
};
}

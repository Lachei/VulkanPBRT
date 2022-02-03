#include"MaterialBehaviors.h"
#include "../Entity.h"

MaterialBehavior::EntityBehaviorBase(Entity entity)
    : _entity(entity)
{
}
void MaterialBehavior::Initialize()
{
    //TODO
}
void MaterialBehavior::Update(float frame_time, MaterialComponent newcomponent)
{
    //frame_time FIXME
    auto& pre_component = _entity.GetComponent<MaterialComponent>();
    pre_component = newcomponent;
}
void MaterialBehavior::Terminate()
{
    //TODO
}
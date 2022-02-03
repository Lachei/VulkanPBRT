#include"LightBehaviors.h"
#include "../Entity.h"

LightBehavior::EntityBehaviorBase(Entity entity)
    : _entity(entity)
{
}
void LightBehavior::Initialize()
{
}
void LightBehavior::Update(float frame_time, LightComponent newcomponent)
{
    //frame_time FIXME
    auto& pre_component = _entity.GetComponent<LightComponent>();
    pre_component = newcomponent;
}
void LightBehavior::Terminate()
{
}
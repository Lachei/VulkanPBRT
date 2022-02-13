#include"LightBehaviors.h"
#include "../Entity.h"

LightBehavior::EntityBehaviorBase(Entity entity)
    : _entity(entity)
{
}
void LightBehavior::Initialize()
{
}
template< typename T >
void LightBehavior::Update(float frame_time, T component)
{
    //TODO
}
void LightBehavior::Terminate()
{
}
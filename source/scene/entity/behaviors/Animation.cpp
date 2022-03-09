#include "Animation.h"
#include "../Components.h"

using namespace vkpbrt;

void AnimationBehavior::Initialize()
{
    // TODO: load animation here
}
void AnimationBehavior::Update(float frame_time)
{
    auto transform = _entity.GetComponent<Transform>();

    // TODO: modify transform based on the loaded animation
}
void AnimationBehavior::Terminate()
{
    
}
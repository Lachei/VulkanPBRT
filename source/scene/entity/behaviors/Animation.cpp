#include "Animation.h"
#include "../Components.h"

using namespace vkpbrt;

void AnimationBehavior::initialize()
{
    // TODO: load animation here
}
void AnimationBehavior::update(float frame_time)
{
    auto* transform = _entity.get_component<Transform>();

    // TODO: modify transform based on the loaded animation
}
void AnimationBehavior::terminate() {}
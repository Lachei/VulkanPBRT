#pragma once
#include "Base.h"

namespace vkpbrt
{
class AnimationBehavior : public EntityBehaviorBase
{
public:
    void initialize() override;
    void update(float frame_time) override;
    void terminate() override;
};
}  // namespace vkpbrt

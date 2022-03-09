#pragma once
#include "Base.h"

namespace vkpbrt
{
class AnimationBehavior : public EntityBehaviorBase
{
public:
    void Initialize() override;
    void Update(float frame_time) override;
    void Terminate() override;
};
}

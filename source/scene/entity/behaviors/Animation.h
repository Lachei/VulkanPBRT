#pragma once
#include "Base.h"

class AnimationBehavior : public EntityBehaviorBase
{
public:
    void Initialize() override;
    void Update(float frame_time) override;
    void Terminate() override;
};
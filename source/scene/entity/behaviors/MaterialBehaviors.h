#pragma once
#include "Base.h"
#include"../Components.h"

class MaterialBehavior : public EntityBehaviorBase
{
public:
    void Initialize() override;
    void Update(float frame_time, MaterialComponent newcomponent) override;
    void Terminate() override;
};

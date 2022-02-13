#pragma once
#include "Base.h"
#include"../Components.h"

class MaterialBehavior : public EntityBehaviorBase
{
public:
    void Initialize() override;
    template< typename T >
    void Update(float frame_time, T newcomponent) override;
    void Terminate() override;
};

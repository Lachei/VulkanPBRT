#pragma once
#include "Base.h"
#include"../Components.h"

class LightBehavior : public EntityBehaviorBase
{
public:
    EntityBehaviorBase(Entity entity);
    virtual void Initialize();
    template< typename T >
    void Update(float frame_time, T component) override;
    virtual void Terminate();
private:
    //LightEntity _entity;
};
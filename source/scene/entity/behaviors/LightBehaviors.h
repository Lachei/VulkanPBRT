#pragma once
#include "Base.h"
#include"../Components.h"
#include "../Entity.h"

class LightBehavior : public EntityBehaviorBase
{
public:
    EntityBehaviorBase(Entity entity);
    virtual void Initialize();
    void Update(float frame_time, LightComponent component) override;
    virtual void Terminate();
private:
    //LightEntity _entity;
};
#include "CameraAnimation.h"
//#include "../Components.h"
#include"../Entity.h"
#include <cmath>

typedef DirectX::SimpleMath::Vector3 position;

AnimationBehavior::AnimationBehavior(Entity entity)
    : _entity(entity)
{
}

void AnimationBehavior::Initialize()
{
    // TODO: load animation here for camera
    work = true;
}
void AnimationBehavior::Update(float frame_time, MODE mode)
{
    auto& tOut = _entity.GetComponent<CameraComponent>().current_Transform;//get current orientation and position
    auto& tStart = pStart_entity.GetComponent().firstFrame_Transform;
    auto& tEnd = _entity.GetComponent().nextFrame_Transform;
    unsigned int steps_num = floor( keyframe_time / frame_time);
    if (mode == MODE::Linear)
    {   
        //pEnd = position of next keyframe
        LinearInterpolate(tOut, tStart, tEnd, steps_num);
    }
    else
    {
        BezierInterpolate(tOut, tStart, tEnd, steps_num);
    }
}
void AnimationBehavior::Terminate()
{
    work = false;
}

void AnimationBehavior::LinearInterpolate(Transform& tOut, const Transform& tStart, const Transform& tEnd, unsigned int steps)
{
    //int8_t steps = 10;
    position& pOut = tOut.position;
    /*position& pStart = tStart.position;
    position& pEnd = tEnd.position;*/
    position step_len = (tEnd.position - tStart.position) / steps;
    unsigned int i = steps;
    while (work == true && i > 0)
    {
        pOut += step_len;
        //TODO: interpolate orientation
        i--;
    }
}

void AnimationBehavior::BezierInterpolate(Transform& tOut, const Transform& tStart, const Transform& tEnd, unsigned int steps)
{
    //int8_t steps = 10;
    float step_len = 1.0 / steps;
    float t = 0.0;
    position& pOut = tOut.position;
    position pStart = tStart.position;
    position pEnd = tEnd.position;
    position p1(pStart.x, pStart.y, 0);
    position p2(pEnd.x, 0, pEnd.z);
    while (work == true && t<1.0)
    {
        pOut = pow(1 - t, 3) * pStart + 3 * pow(1 - t, 2) * t * p1 + 3 * (1 - t) * pow(t, 2) * p2 + pow(t, 3) * pEnd;
        //TODO: interpolate orientation
        t += step_len;
    }
}

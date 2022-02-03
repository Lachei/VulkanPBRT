#include "CameraAnimation.h"
//#include "../Components.h"
#include"../Entity.h"
#include <cmath>

typedef DirectX::SimpleMath::Vector3 Position;
typedef DirectX::SimpleMath::Quaternion Orientation;
AnimationBehavior::AnimationBehavior(Entity entity)
    : _entity(entity)
{
}

void AnimationBehavior::Initialize()
{
    // TODO: load animation here for camera
    work = true;
    tOut = _entity.GetComponent<CameraComponent>().current_Transform;//get current orientation and position
    tEnd = _entity.GetComponent().nextFrames_Transform.begin();
}
void AnimationBehavior::Update(float frame_time, MODE mode)
{
    //auto& tOut = _entity.GetComponent<CameraComponent>().current_Transform;//get current orientation and position
    //auto& tEnd = _entity.GetComponent().nextFrames_Transform.begin();
    unsigned int steps_num = floor( keyframe_time / frame_time);
    if (mode == MODE::Linear)
    {   
        while (work == true && tEnd != pStart_entity.GetComponent().nextFrames_Transform.end())
        {
            Transform tStart = tOut;
            LinearInterpolate(tOut, tStart, tEnd, steps_num);
            ++tEnd;
        }
        
    }
    else
    {
        while (work == true && tEnd != pStart_entity.GetComponent().nextFrames_Transform.end())
        {
            Transform tStart = tOut;
            BezierInterpolate(tOut, tStart, tEnd, steps_num);
            ++tEnd;
        }
    }
}
void AnimationBehavior::Terminate()
{
    work = false;
}

void AnimationBehavior::LinearInterpolate(Transform& tOut, const Transform& tStart, const Transform& tEnd, unsigned int steps)
{
    //int8_t steps = 10;
    Position& pOut = tOut.position;
    Orientation& oOut = tOut.orientation;
    Orientation& oStart = tStart.orientation;
    Orientation& oEnd = tEnd.orientation;
    /*position& pStart = tStart.position;
    position& pEnd = tEnd.position;*/
    Position step_len = (tEnd.position - tStart.position) / steps;
    unsigned int i = 0;
    while (work == true && i < steps)
    {
        pOut += step_len;
        //TODO: interpolate orientation
        using namespace DirectX::SimpleMath;
        oOut = Slerp(oStart, oEnd, i / (float)steps);
        i++;
    }
}
typedef DirectX::SimpleMath::Vector3 Position;
typedef DirectX::SimpleMath::Quaternion Orientation;
void AnimationBehavior::BezierInterpolate(Transform& tOut, const Transform& tStart, const Transform& tEnd, unsigned int steps)
{
    //int8_t steps = 10;
    float step_len = 1.0 / steps;
    float t = 0.0;
    Position& pOut = tOut.position;
    Position pStart = tStart.position;
    Position pEnd = tEnd.position;
    Position p1(pStart.x, pStart.y, 0);
    Position p2(pEnd.x, 0, pEnd.z);
    Orientation& oOut = tOut.orientation;
    Orientation& oStart = tStart.orientation;
    Orientation& oEnd = tEnd.orientation;
    while (work == true && t<1.0)
    {
        pOut = pow(1 - t, 3) * pStart + 3 * pow(1 - t, 2) * t * p1 + 3 * (1 - t) * pow(t, 2) * p2 + pow(t, 3) * pEnd;
        //TODO: interpolate orientation
        using namespace DirectX::SimpleMath;
        oOut = Slerp(oStart, oEnd, t);
        t += step_len;
    }
}


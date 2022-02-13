#include "CameraAnimation.h"
//#include "../Components.h"
#include"../Entity.h"
#include <cmath>
#include "SimpleMath.h"



void AnimationBehavior::Initialize()
{
    // TODO: load animation here for camera
    //get trf of all keyframes through GetComponents
    ////KeyframeComponents is datatype, list of trfs is one component of camera entity
    ////before you can load  them here, you should read from json file by read() function and add this component through EntityManager
    transform_list = _entity.GetComponent<std::list<Transform>>();//pointer
    //current transform, which is another component of camera entity
    try
    {
        //assign element of first trf to current_transform
        _entity.AddComponent<Transform>(transform_list.front()); //pointer;// vector::at throws an out-of-range
    }
    catch (const std::out_of_range& oor) {
        std::cerr << "Out of Range error: " << oor.what() << '\n';
    }
    current_transform = _entity.GetComponent<Transform>();
    //number of keyframes
    keyframe_time = total_time/transform_list.size();

}

void AnimationBehavior::Update(float frame_time, MODE mode)
{
    unsigned int frame_num = floor( keyframe_time / frame_time);
    auto last_keyframe = transform_list.front();
    transform_list.pop_front();
    auto next_keyframe = transform_list.front();
    while (!transform_list.empty())
    {
        next_keyframe = transform_list.front();
        if (mode == MODE::Linear)
            LinearInterpolate(current_transform, last_keyframe, next_keyframe, frame_num);
        else
            BezierInterpolate(current_transform, last_keyframe, next_keyframe, frame_num);
        last_keyframe = transform_list.front();
        transform_list.pop_front();
    }
}

void AnimationBehavior::LinearInterpolate(Transform& current_transform, const Transform& last_keyframe, const Transform& next_keyframe, unsigned int frame_num)
{
    using namespace DirectX::SimpleMath;
    Vector3 step_len = (next_keyframe.position - last_keyframe.position) / frame_num;
    unsigned int i = 0;
    while( i < frame_num)
    {
        current_transform.position += step_len;
        //TODO: interpolate orientation
        current_transform.orientation = Slerp(last_keyframe.orientation, next_keyframe.orientation, i / (float)frame_num);
        i++;
    }
}

void AnimationBehavior::BezierInterpolate(Transform& current_transform, const Transform& last_keyframe, const Transform& next_keyframe, unsigned int frame_num)
{
    //int8_t steps = 10;
    float step_len = 1.0 / frame_num;
    float t = 0.0;
    using namespace DirectX::SimpleMath;

    Vector3 p1(last_keyframe.position.x, last_keyframe.position.y, 0);
    Vector3 p2(next_keyframe.position.x, 0, next_keyframe.position.z);
    while(t<1.0)
    {
        current_transform.position = pow(1 - t, 3) * last_keyframe.position + 3 * pow(1 - t, 2) * t * p1 + 3 * (1 - t) * pow(t, 2) * p2 + pow(t, 3) * next_keyframe.position;
        //TODO: interpolate orientation
        using namespace DirectX::SimpleMath;
        current_transform.orientation = Slerp(current_transform.orientation, current_transform.orientation, t);
        t += step_len;
    }
}


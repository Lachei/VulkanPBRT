#pragma once
#include "Base.h"
#include "../Components.h"

enum class MODE { Linear, Bezier};

class AnimationBehavior : public EntityBehaviorBase
{
public:
    void Initialize() override;
    void Update(float frame_time, MODE mode) override;
    //void Terminate() override;
//protected:
//    /**
//     * \brief The entity this behavior is attached to.
//     */
//    Entity camera_entity;
private:
    //bool work;
    float total_time;
    float keyframe_time;
    Transform& current_transform;//one of component of camera
    std::list<Transform>& transform_list; //another component of camera, which is a list saves all transforms of keyframes
    //Transform& tEnd;
    void LinearInterpolate(Transform& current_transform, const Transform& last_keyframe, const Transform& next_keyframe, unsigned int steps);
    void BezierInterpolate(Transform& current_transform, const Transform& last_keyframe, const Transform& next_keyframe, unsigned int steps);
};
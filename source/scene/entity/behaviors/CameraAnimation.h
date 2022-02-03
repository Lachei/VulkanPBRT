#pragma once
#include "Base.h"
#include "Components.h"
enum class MODE { Linear, Bezier};
typedef DirectX::SimpleMath::Vector3 Position;
typedef DirectX::SimpleMath::Quaternion Orientation;
class AnimationBehavior : public EntityBehaviorBase
{
public:
    void Initialize() override;
    void Update(float frame_time, MODE mode= MODE::Linear) override;
    void Terminate() override;
private:
    bool work;
    Transform& tOut;
    Transform& tEnd;
    void LinearInterpolate(Transform& tOut, const Transform& tStart, const Transform& tEnd, unsigned int steps);
    void BezierInterpolate(Transform& tOut, const Transform& tStart, const Transform& tEnd, unsigned int steps);
};
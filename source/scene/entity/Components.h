#pragma once
#include "SimpleMath.h"

struct Transform
{
    DirectX::SimpleMath::Quaternion orientation;
    DirectX::SimpleMath::Vector3 position;
    DirectX::SimpleMath::Vector3 scale;
};
struct CameraComponent
{
    Transform current_Transform;
    Transform firstFrame_Transform;
    Transform nextFrame_Transform;//status of next keyframe
};

enum class LightTYPE { POINT, SPOT, AMBIENT, DIRECTIONAL};
struct LightComponent
{
    LightTYPE type;
    DirectX::SimpleMath::Quaternion direction;
    DirectX::SimpleMath::Vector3 position;
    DirectX::SimpleMath::Vector3 color;
    float energy;
};

struct MaterialComponent {
    DirectX::SimpleMath::Vector3 rgb;
    float diffuse;
    float specular;
    float ambient;
    float emissive;
};

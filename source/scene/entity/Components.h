#pragma once
#include "SimpleMath.h"

struct Transform
{
    DirectX::SimpleMath::Quaternion orientation;
    DirectX::SimpleMath::Vector3 position;
    DirectX::SimpleMath::Vector3 scale;
};

typedef std::vector<Transform> keyframeCameraData;
struct CameraComponent
{
    Transform current_Transform;
    keyframeCameraData nextFrames_Transform;
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

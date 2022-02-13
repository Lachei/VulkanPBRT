#pragma once
#include "SimpleMath.h"

struct Transform
{
    DirectX::SimpleMath::Quaternion orientation;
    DirectX::SimpleMath::Vector3 position;
    DirectX::SimpleMath::Vector3 scale;
};

enum class LightTYPE { POINT, SPOT, AMBIENT, DIRECTIONAL};


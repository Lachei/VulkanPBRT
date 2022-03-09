#pragma once
#include "SimpleMath.h"
#include "behaviors/Base.h"

namespace vkpbrt
{
struct Transform
{
    DirectX::SimpleMath::Quaternion orientation;
    DirectX::SimpleMath::Vector3 position;
    DirectX::SimpleMath::Vector3 scale;
};

struct Camera
{
    float focal_length;
    float near_clip;
    float far_clip;
};

struct Material
{
    DirectX::SimpleMath::Vector3 albedo;
    float roughness;
    float metalness;
    // TODO: figure out a better material model that includes textures
};

struct Mesh
{
    uint64_t mesh_id;
};

struct Behavior
{
    EntityBehaviorBase* entity_behavior;
};
}

#pragma once
#include "SimpleMath.h"
#include "behaviors/Base.h"
#include "ComponentRegistry.h"

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
    // TODO: add texture IDs
};

struct Mesh
{
    uint64_t mesh_id;
};

struct Behavior
{
    EntityBehaviorBase* entity_behavior;
};

}  // namespace vkpbrt

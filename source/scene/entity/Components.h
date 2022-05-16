#pragma once
#include "SimpleMath.h"
#include "behaviors/Base.h"

namespace vkpbrt
{
struct TransformComponent
{
    DirectX::SimpleMath::Quaternion orientation;
    DirectX::SimpleMath::Vector3 position;
    DirectX::SimpleMath::Vector3 scale;
};

struct CameraComponent
{
    float focal_length;
    float near_clip;
    float far_clip;
};

struct MaterialComponent
{
    DirectX::SimpleMath::Vector3 albedo;
    float roughness;
    float metalness;
    // TODO: add texture IDs
};

struct MeshComponent
{
    uint64_t mesh_resource_id;
};

struct BehaviorComponent
{
    EntityBehaviorBase* entity_behavior;
};

}  // namespace vkpbrt

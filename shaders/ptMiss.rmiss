#version 460
#extension GL_EXT_ray_tracing : enable

#include "ptStructures.glsl"

layout(location = 1) rayPayloadInEXT RayPayload rayPayload;

void main()
{
    rayPayload.position = vec3(1.0e10);
    rayPayload.si.normal = vec3(1);
    rayPayload.si.emissiveColor = vec3(0);
}
#version 460
#extension GL_EXT_ray_tracing : enable

#include "general.glsl"

layout(location = 1) rayPayloadInEXT RayPayload rayPayload;

void main()
{
    rayPayload.color = vec3(0);
    rayPayload.albedo = vec4(0);
    rayPayload.position = vec3(1.0e10);
    rayPayload.si.normal = vec3(1);
    rayPayload.reflector = 0;
}

#version 460
#extension GL_EXT_ray_tracing : enable

#include "general.glsl"

layout(location = 1) rayPayloadInEXT RayPayload rayPayload;

void main()
{
    rayPayload.color = vec3(0,0,.2f);
    rayPayload.position = vec3(1.0/0); //inf
    rayPayload.normal = vec3(0,0,1);
    rayPayload.reflector = 0;
}

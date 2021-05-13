#version 460
#extension GL_EXT_ray_tracing : enable

#include "general.glsl"

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;

void main()
{
    rayPayload.color = vec3(0,0,.2f);
    rayPayload.distance = 1.0/0; //inf
    rayPayload.normal = rayPayload.color;
    rayPayload.reflector = 0;
}

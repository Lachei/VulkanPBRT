#version 460
#extension GL_EXT_ray_tracing : enable

#include "general.glsl"

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;

void main()
{
    rayPayload.color = vec4(0,0,.2f,1);
    rayPayload.distance = 1.0/0; //inf
    rayPayload.normal = vec3(0,0,1);
    rayPayload.reflector = 0;
}

#version 460
#extension GL_EXT_ray_tracing : enable

#include "ptStructures.glsl"

layout(location = 1) rayPayloadInEXT RayPayload rayPayload;

void main()
{
    rayPayload.position = vec3(1.0e10);
    rayPayload.category_id = uint(-1);
    rayPayload.si.normal = vec3(1);
    rayPayload.si.emissiveColor = vec3(0);
    rayPayload.si.diffuseColor = vec3(0);
    rayPayload.si.specularColor = vec3(0);
    rayPayload.si.perceptualRoughness = 0;
    rayPayload.si.metalness = 0;
    rayPayload.si.alphaRoughness = 0;
    rayPayload.si.illuminationType = 0;
    rayPayload.si.reflectance0 = vec3(0);
    rayPayload.si.reflectance90 = vec3(0);
    rayPayload.si.transmissiveColor = vec3(0);
    rayPayload.si.basis = mat3(0);
    rayPayload.si.indexOfRefraction = 1;
}
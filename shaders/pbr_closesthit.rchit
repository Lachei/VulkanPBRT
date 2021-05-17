#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(binding = 0, set = 0) uniform accelerationStructureEXT tlas;
layout(binding = 15, set = 0) uniform UBO{
  mat4 inverseViewMatrix;
	mat4 inverseProjectionMatrix;
}ubo;
layout(binding = 2, set = 0) buffer Pos {float p[]; }     pos[];  //non interleaved positions, normals and texture arrays
layout(binding = 3, set = 0) buffer Normals {float n[]; } nor[];
layout(binding = 4, set = 0) buffer Tex {float t[]; }     tex[];
layout(binding = 5, set = 0) buffer Indices {float i[]; } ind[];
layout(binding = 6) uniform sampler2D diffuseMap[];
layout(binding = 7) uniform sampler2D mrMap[];
layout(binding = 8) uniform sampler2D normalMap[];
layout(binding = 9) uniform sampler2D aoMap[];
layout(binding = 10) uniform sampler2D emissiveMap[];
layout(binding = 11) uniform sampler2D specularMap[];
#define VERTEXINFOAVAILABLE
#include "general.glsl"
layout(binding = 12) buffer Lights{int len; int padding[3];Light l[]; } lights;
layout(binding = 13) buffer Materials{WaveFrontMaterialPacked m[]; } materials;
layout(binding = 14) buffer Instances{ObjectInstance i[]; } instances;


layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
hitAttributeEXT vec2 attribs;

void main()
{
  uint objId = int(instances.i[gl_InstanceCustomIndexEXT].meshId);
  ivec3 index = ivec3(ind[objId].i[3 * gl_PrimitiveID], ind[objId].i[3 * gl_PrimitiveID + 1], ind[objId].i[3 * gl_PrimitiveID + 2]);

  Vertex v0 = unpack(index.x, objId);
	Vertex v1 = unpack(index.y, objId);
	Vertex v2 = unpack(index.z, objId);

  const vec3 bar = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
  vec3 normal = normalize(v0.normal * bar.x + v1.normal * bar.y + v2.normal * bar.z);
  rayPayload.color = vec3(1);
  rayPayload.normal = normal;
  rayPayload.distance = gl_RayTmaxEXT;
  rayPayload.reflector = 1;
}

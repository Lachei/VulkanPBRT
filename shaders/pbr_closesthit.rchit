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
layout(binding = 5, set = 0) buffer Indices {uint i[]; }  ind[];
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
  uint indexStride = int(instances.i[gl_InstanceCustomIndexEXT].indexStride);
  uvec3 index;
  if(indexStride == 4)  //full uints are in the indexbuffer
    index = ivec3(ind[nonuniformEXT(objId)].i[3 * gl_PrimitiveID], ind[nonuniformEXT(objId)].i[3 * gl_PrimitiveID + 1], ind[nonuniformEXT(objId)].i[3 * gl_PrimitiveID + 2]);
  else                  //only shorts are in the indexbuffer
  {
    int full = 3 * gl_PrimitiveID;
    uint p = uint(full * .5f);
    if(bool(full & 1)){   //not dividable by 2, second half of p + bot places of p + 1
      index.x = ind[nonuniformEXT(objId)].i[p] & 0xffff;
      uint t = ind[nonuniformEXT(objId)].i[p + 1];
      index.y = t >> 16;
      index.z = t & 0xffff;
    }
    else{           //dividable by 2, full p plus first halv of p + 1
      uint t = ind[nonuniformEXT(objId)].i[p];
      index.x = t >> 16;
      index.y = t & 0xffff;
      index.z = ind[nonuniformEXT(objId)].i[p + 1] >> 16;
    }
  }

  Vertex v0 = unpack(index.x, objId);
	Vertex v1 = unpack(index.y, objId);
	Vertex v2 = unpack(index.z, objId);

  const vec3 bar = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
  vec3 normal = normalize(v0.normal * bar.x + v1.normal * bar.y + v2.normal * bar.z);
  vec3 position = v0.pos * bar.x + v1.pos * bar.y + v2.pos * bar.z;
  vec2 texCoord = v0.uv * bar.x + v1.uv * bar.y + v2.uv * bar.z;
  vec3 albedo = texture(diffuseMap[nonuniformEXT(objId)], texCoord).xyz;
  rayPayload.color = position;
  rayPayload.normal = normal;
  rayPayload.distance = gl_RayTmaxEXT;
  rayPayload.reflector = 1;
}

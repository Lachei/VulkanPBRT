#ifndef GENERAL
#define GENERAL

struct Vertex{
    vec3 pos;
    vec3 normal;
    vec2 uv;
};

struct RayPayload {
	vec3 color;
	float distance;
	vec3 normal;
	float reflector;
};

struct WaveFrontMaterialPacked
{
  vec4  ambientShininess;
  vec4  diffuseIor;
  vec4  specularDissolve;
  vec4  transmittanceIllum;
  vec4  emissionTextureId;
  //float shininess;
  //float ior;       // index of refraction
  //float dissolve;  // 1 == opaque; 0 == fully transparent
  //int   illum;     // illumination model (see http://www.fileformat.info/format/material/)
  //int   textureId;
};

struct ObjectInstance{
  mat4 objectMat;
  int meshId;
  int pad[3];
};

struct WaveFrontMaterial
{
  vec3  ambient;
  vec3  diffuse;
  vec3  specular;
  vec3  transmittance;
  vec3  emission;
  float shininess;
  float ior;       // index of refraction
  float dissolve;  // 1 == opaque; 0 == fully transparent
  int   illum;     // illumination model (see http://www.fileformat.info/format/material/)
  int   textureId;
};

// a light is generally defined as a plane with 3 verts, a light type, a direction, strength, angle and radius
// with this emissive planes, point lights with varying radii, cone lights and directional lights can be simulated
struct Light{
    vec4 v0Type;
    vec4 v1Strength;
    vec4 v2Angle;
    vec4 dirAngle2;
    vec4 colRadius;
};

#ifdef VERTEXINFOAVAILABLE
Vertex unpack(uint index, uint objId){
    Vertex v;
    v.pos.x = pos[objId].p[3 * index];
    v.pos.y = pos[objId].p[3 * index + 1];
    v.pos.z = pos[objId].p[3 * index + 2];
    v.normal.x = nor[objId].n[3 * index];
    v.normal.y = nor[objId].n[3 * index + 1];
    v.normal.z = nor[objId].n[3 * index + 2];
    v.uv.x = tex[objId].t[2 * index];
    v.uv.y = tex[objId].t[2 * index + 1];

    return v;
};
#endif

WaveFrontMaterial unpack(WaveFrontMaterialPacked p){
    WaveFrontMaterial m;
    m.ambient = p.ambientShininess.xyz;
    m.diffuse = p.diffuseIor.xyz;
    m.specular = p.specularDissolve.xyz;
    m.transmittance = p.transmittanceIllum.xyz;
    m.emission = p.emissionTextureId.xyz;
    m.shininess = p.ambientShininess.w;
    m.ior = p.diffuseIor.w;
    m.dissolve = p.specularDissolve.w;
    m.illum = int(p.transmittanceIllum.w);
    m.textureId = int(p.emissionTextureId.w);
    return m;
};

#endif
#ifndef PTSTRUCTURES_H
#define PTSTRUCTURES_H

struct Vertex{
    vec3 pos;
    vec3 normal;
    vec2 uv;
};

struct WaveFrontMaterialPacked
{
  vec4  ambientShininess;
  vec4  diffuseIor;
  vec4  specularDissolve;
  vec4  transmittanceIllum;
  vec4  emissionTextureId;
  uint category_id;
  float pad[3];
};

struct ObjectInstance{
  mat4 objectMat;
  int meshId;
  uint indexStride;
  int pad[2];
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
  float alphaCutoff;
  uint category_id;
};

// Light source types(lst)
const uint lst_undefined = 0;
const uint lst_directional = 1;
const uint lst_point = 2;
const uint lst_spot = 3;
const uint lst_ambient = 4;
const uint lst_area = 5;

// a light is generally defined as a plane with 3 verts, a light type, a direction, strength, angle and radius
// with this emissive planes, point lights with varying radii, cone lights and directional lights can be simulated
struct Light{
    vec4 v0Type;
    vec4 v1Strength;
    vec4 v2Angle;
    vec4 dirAngle2;
    vec4 colAmbient;
    vec4 colDiffuse;
    vec4 colSpecular;
    vec4 strengths;
};

// Encapsulate the various inputs used by the various functions in the shading equation
// We store values in this struct to simplify the integration of alternative implementations
// of the shading terms, outlined in the Readme.MD Appendix.
struct SurfaceInfo{
    float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
    float metalness;              // metallic value at the surface
    float alphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 reflectance0;            // full reflectance color (normal incidence angle)
    vec3 reflectance90;           // reflectance color at grazing angle
    vec3 diffuseColor;            // color contribution from diffuse lighting
    vec3 specularColor;           // color contribution from specular lighting
    vec3 emissiveColor;
    vec3 normal;
    mat3 basis;                   // tbn matrix for converting form object to world space
};

struct PBRInfo
{
    float NdotL;                  // cos angle between normal and light direction
    float NdotV;                  // cos angle between normal and view direction
    float NdotH;                  // cos angle between normal and half vector
    float LdotH;                  // cos angle between light direction and half vector
    float VdotH;                  // cos angle between view direction and half vector
    float VdotL;                  // cos angle between view direction and light direction
    SurfaceInfo s;                // surface information
};

struct RayPayload {
	vec3 position;
    SurfaceInfo si;
    uint category_id;
};

#endif //PTSTRUCTURES_H
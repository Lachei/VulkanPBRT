#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable

layout(binding = 0, set = 0) uniform accelerationStructureEXT tlas;
layout(binding = 2, set = 0) buffer Pos {float p[]; }     pos[];  //non interleaved positions, normals and texture arrays
layout(binding = 3, set = 0) buffer Nor {float n[]; }     nor[];
layout(binding = 4, set = 0) buffer Tex {float t[]; }     tex[];
layout(binding = 5, set = 0) buffer Ind {uint i[]; }  ind[];
layout(binding = 6) uniform sampler2D diffuseMap[];
layout(binding = 7) uniform sampler2D mrMap[];
layout(binding = 8) uniform sampler2D normalMap[];
layout(binding = 10) uniform sampler2D emissiveMap[];
layout(binding = 11) uniform sampler2D specularMap[];
#define VERTEXINFOAVAILABLE
#define TEXTURESAVAILABLE
#include "general.glsl"
layout(binding = 12) buffer Lights{Light l[]; } lights;
layout(binding = 13) buffer Materials{WaveFrontMaterialPacked m[]; } materials;
layout(binding = 14) buffer Instances{ObjectInstance i[]; } instances;
layout(binding = 26) uniform Infos{
  uint lightCount;
  uint minRecursionDepth;
  uint maxRecursionDepth;
}infos;


layout(location = 0) rayPayloadEXT bool shadowed;
layout(location = 1) rayPayloadInEXT RayPayload rayPayload;
hitAttributeEXT vec2 attribs;

//return light color(area foreshortening already included)
//only light which can contribute are considered, priority sampling over light strength
//uses weighted reservoir sampling to only have to go through the lights once
vec3 sampleLight(vec3 pos, vec3 n, out vec3 l, out float pdf)
{
  pdf = 0;
  l = vec3(0);
  float strengthSum = 0; //holds the summed up light contributions
  float pickedStrength = 0;
  vec3 pickedLightStrength;
  float tmax = 1000.0;
  float tmin = 0.001;
  //summing up all light strengths
  for(int i = 0; i < infos.lightCount; ++i){
    float lightPower = dot(lights.l[i].colAmbient + lights.l[i].colDiffuse + lights.l[i].colSpecular, vec4(1));
    float strength = 0;
    vec3 lightStrength = lights.l[i].colAmbient.xyz + lights.l[i].colDiffuse.xyz + lights.l[i].colSpecular.xyz;
    float d = 0, attenuation = 0;
    float curTmax = 1000.0;
    vec3 curL;
    switch(int(lights.l[i].v0Type.w)){
      case lst_directional:
        d = distance(pos, lights.l[i].v0Type.xyz);
        attenuation = 1.0f / (lights.l[i].strengths.x + lights.l[i].strengths.y * d + lights.l[i].strengths.z * d * d);
        strength = max(dot(n, -lights.l[i].dirAngle2.xyz), 0) * lightPower * attenuation;
        lightStrength *= dot(n, -lights.l[i].dirAngle2.xyz) * attenuation;
        curL = normalize(-lights.l[i].dirAngle2.xyz);
        break;
      case lst_point:
        d = distance(pos, lights.l[i].v0Type.xyz);
        attenuation = 1.0f / (lights.l[i].strengths.x + lights.l[i].strengths.y * d + lights.l[i].strengths.z * d * d);
        strength = max(dot(n, normalize(lights.l[i].v0Type.xyz - pos)), 0) * lightPower * attenuation;
        lightStrength *= dot(n, normalize(lights.l[i].v0Type.xyz - pos)) * attenuation;
        curL = normalize(lights.l[i].v0Type.xyz - pos);
        break;
      case lst_spot:

        break;
      case lst_ambient:

        break;
      case lst_area:
        //sample triangle position
        vec2 barycentrics = sampleTriangle(randomVec2(rayPayload.re));
        vec3 p1 = lights.l[i].v0Type.xyz;
        vec3 p2 = lights.l[i].v1Strength.xyz;
        vec3 p3 = lights.l[i].v2Angle.xyz;
        vec3 lightP = blerp(barycentrics, p1, p2, p3);
        vec3 lightDir = lightP - pos;
        vec3 lightNormal = cross(p2 - p1, p3 - p1);
        float triangleArea = .5f * length(lightNormal);
        lightNormal = normalize(lightNormal);
        d = length(lightDir);
        lightDir /= d;
        attenuation = 1.0f / (lights.l[i].strengths.x + lights.l[i].strengths.y * d + lights.l[i].strengths.z * d * d);
        strength = max(dot(n, lightDir) * abs(dot(lightDir, lightNormal)), 0) * lightPower * attenuation * triangleArea;
        lightStrength *= dot(n, lightDir) * abs(dot(lightDir, lightNormal)) * attenuation * triangleArea;
        curL = lightDir;
        curTmax = d - tmin;
        break;
    }
    strengthSum += strength;
    if(randomFloat(rayPayload.re) < strength / strengthSum){   //update selected light
      pickedStrength = strength;
      pickedLightStrength = lightStrength;
      tmax = curTmax;
      l = curL;
    }
  }

  if(strengthSum == 0){   //surface is not directly lit
    pdf = 0;
    l = vec3(0);
    return vec3(0);
  }

  pdf = pickedStrength / strengthSum;
  vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
  shadowed = true;
  traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsNoOpaqueEXT, 0xFF, 0, 0, 1, origin, tmin, l, tmax, 0);
  return pickedLightStrength * float(!shadowed);
}

//calculates direct lighting on the surface point at pos
vec3 nextEventEsitmation(vec3 pos, vec3 o, SurfaceInfo s){
  vec3 light  = vec3(0);

  vec3 l;
  float lightPdf;
  vec3 lightCol = sampleLight(pos, s.normal, l, lightPdf);
  if(lightCol == vec3(0)) return lightCol;
  vec3 lh = normalize(l + o);
  float cosTheta = dot(l, s.normal);
  vec3 brdf = BRDF(o, l, lh, s);
  float brdfPdf = pdfBRDF(s, o, l, lh);
  float weight = powerHeuristics(lightPdf, brdfPdf);
  light += lightCol * brdf * weight / lightPdf;   //no multiplication with cosTheta as it is already done in sampleLight()
  return min(rayPayload.throughput * light, vec3(c_MaxRadiance));
}

void main()
{
  const float epsilon = 1e-6;
  ObjectInstance instance = instances.i[gl_InstanceCustomIndexEXT];
  uint objId = int(instance.meshId);
  uint indexStride = int(instances.i[gl_InstanceCustomIndexEXT].indexStride);
  uvec3 index;
  if(indexStride == 4)  //full uints are in the indexbuffer
    index = ivec3(ind[nonuniformEXT(objId)].i[3 * gl_PrimitiveID], ind[nonuniformEXT(objId)].i[3 * gl_PrimitiveID + 1], ind[nonuniformEXT(objId)].i[3 * gl_PrimitiveID + 2]);
  else                  //only ushorts are in the indexbuffer
  {
    int full = 3 * gl_PrimitiveID;
    uint p = uint(full * .5f);
    if(bool(full & 1)){   //not dividable by 2, second half of p + both places of p + 1
      index.x = ind[nonuniformEXT(objId)].i[p] >> 16;
      uint t = ind[nonuniformEXT(objId)].i[p + 1];
      index.z = t >> 16;
      index.y = t & 0xffff;
    }
    else{           //dividable by 2, full p plus first halv of p + 1
      uint t = ind[nonuniformEXT(objId)].i[p];
      index.y = t >> 16;
      index.x = t & 0xffff;
      index.z = ind[nonuniformEXT(objId)].i[p + 1] & 0xffff;
    }
  }

  Vertex v0 = unpack(index.x, objId);
	Vertex v1 = unpack(index.y, objId);
	Vertex v2 = unpack(index.z, objId);

  const vec3 bar = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
  vec2 texCoord = v0.uv * bar.x + v1.uv * bar.y + v2.uv * bar.z;
  vec4 diffuse = SRGBtoLINEAR(texture(diffuseMap[nonuniformEXT(objId)], texCoord));
  vec3 position = v0.pos * bar.x + v1.pos * bar.y + v2.pos * bar.z;
  position = (instance.objectMat * vec4(position, 1)).xyz;
  vec3 normal = normalize(v0.normal * bar.x + v1.normal * bar.y + v2.normal * bar.z).xyz;//.xzy;
  mat4 normalObj = transpose(inverse(instance.objectMat));
  normal = normalize((normalObj * vec4(normal, 0)).xyz);
  if(v0.uv == v1.uv) v1.uv += vec2(epsilon,0);
  if(v0.uv == v2.uv) v2.uv += vec2(0,2 * epsilon);
  if(v1.uv == v2.uv) v2.uv += vec2(epsilon);
  vec3 T = (normalObj * vec4(getTangent(v0.pos, v1.pos, v2.pos, v0.uv, v1.uv, v2.uv).xyz, 0)).xyz;
  //T = (instance.objectMat * vec4(T, 0)).xyz;
  vec3 B = (normalObj * vec4(getBitangent(v0.pos, v1.pos, v2.pos, v0.uv, v1.uv, v2.uv).xyz, 0)).xyz;
  //B = (instance.objectMat * vec4(B, 0)).xyz;
  mat3 TBN = gramSchmidt(T, B, normal);
  normal = getNormal(TBN, normalMap[nonuniformEXT(objId)], texCoord);

  WaveFrontMaterial mat = unpack(materials.m[objId]);
  float perceptualRoughness = 0;
  float metallic;
  vec4 baseColor = diffuse * vec4(mat.ambient, 1);
  vec4 albedo = baseColor;

  const vec3 f0 = vec3(.04);

  vec4 specular;
  if(textureSize(specularMap[nonuniformEXT(objId)], 0) == ivec2(1,1))
    specular = vec4(0,0,0,0);
  else
    SRGBtoLINEAR(texture(specularMap[nonuniformEXT(objId)], texCoord));
  perceptualRoughness = 1.0 - specular.a;

  float maxSpecular = max(max(specular.r, specular.g), specular.b);

  metallic = convertMetallic(diffuse.rgb, specular.rgb, maxSpecular);

  vec3 baseColorDiffusePart = diffuse.rgb * ((1.0 - maxSpecular) / (1 - c_MinRoughness) / max(1 - metallic, epsilon)) * mat.ambient.rgb;
  vec3 baseColorSpecularPart = specular.rgb - (vec3(c_MinRoughness) * (1 - metallic) * (1 / max(metallic, epsilon))) * mat.specular.rgb;
  baseColor = vec4(mix(baseColorDiffusePart, baseColorSpecularPart, metallic * metallic), diffuse.a);

  vec3 diffuseColor = baseColor.rgb * (vec3(1.0) - f0);
  diffuseColor *= 1.0 - metallic;

  float alphaRoughness = perceptualRoughness * perceptualRoughness;
  vec3 specularColor = mix(f0, baseColor.rgb, metallic);

  float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

  float reflectance90 = clamp(reflectance * 25, 0, 1);
  vec3 specularEnvironmentR0 = specularColor.rgb;
  vec3 specularEnvironmentR90 = vec3(1) * reflectance90;
  vec3 v = normalize(-gl_WorldRayDirectionEXT);

  rayPayload.si = SurfaceInfo(perceptualRoughness, metallic, specularEnvironmentR0, specularEnvironmentR90, alphaRoughness, diffuseColor, specularColor, normal, TBN);

  //direct illumination
  rayPayload.color = nextEventEsitmation(position, v, rayPayload.si);
  //surface emission
  vec3 emissive = mat.emission;// * SRGBtoLINEAR(texture(emissiveMap[nonuniformEXT(objId)], texCoord)).rgb;
  rayPayload.color += emissive;

  rayPayload.albedo = vec4(specularColor + diffuseColor, 1);
  rayPayload.position = position;
  rayPayload.reflector = 1;
}

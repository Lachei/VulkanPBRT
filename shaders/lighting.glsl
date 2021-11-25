#ifndef LIGHTING_H
#define LIGHTING_H

#include "math.glsl"

float powerHeuristics(float a, float b){
    float f = a * a;
    float g = b * b;
    return f / (f + g);
}

//return light color(area foreshortening already included)
//only light which can contribute are considered, priority sampling over light strength
//uses weighted reservoir sampling to only have to go through the lights once
vec3 sampleLight(vec3 pos, vec3 n, inout RandomEngine re, out vec3 l, out float pdf)
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
        vec2 barycentrics = sampleTriangle(randomVec2(re));
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
        strength = max(dot(n, lightDir), 0) * max(dot(-lightDir, lightNormal), 0) * lightPower * attenuation * triangleArea;
        lightStrength *= max(dot(n, lightDir), 0) * max(dot(-lightDir, lightNormal), 0) * attenuation * triangleArea;
        curL = lightDir;
        curTmax = d - tmin;
        break;
    }
    strengthSum += strength;
    if(randomFloat(re) < strength / strengthSum){   //update selected light
      pickedStrength = strength;
      pickedLightStrength = lightStrength;
      tmax = curTmax;
      l = curL;
    }
  }

  if(strengthSum < 1e-6){   //surface is not directly lit
    pdf = 0;
    l = vec3(0);
    return vec3(0);
  }

  pdf = pickedStrength / strengthSum;
  shadowed = true;
  traceRayEXT(tlas, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsNoOpaqueEXT, 0xFF, 0, 0, 1, pos, tmin, l, tmax, 0);
  return pickedLightStrength * float(!shadowed);
}

//calculates direct lighting on the surface point at pos
vec3 nextEventEsitmation(vec3 pos, vec3 o, SurfaceInfo s, vec3 throughput, inout RandomEngine re){
  if(s.normal == vec3(1,1,1)) return vec3(0);
  vec3 light  = vec3(0);

  vec3 l;
  float lightPdf;
  vec3 lightCol = sampleLight(pos, s.normal, re, l, lightPdf);
  if(lightCol == vec3(0)) return lightCol;
  vec3 lh = normalize(l + o);
  float cosTheta = dot(l, s.normal);
  vec3 brdf = BRDF(o, l, lh, s);
  float brdfPdf = pdfBRDF(s, o, l, lh);
  float weight = powerHeuristics(lightPdf, brdfPdf);
  light += lightCol * brdf * weight / lightPdf;   //no multiplication with cosTheta as it is already done in sampleLight()
  return min(throughput * light, vec3(c_MaxRadiance));
}

vec3 indirectLighting(vec3 pos, vec3 v, SurfaceInfo s, int recDepth, inout vec3 throughput, inout RandomEngine re){
  vec3 l;
  float pdf;
  vec3 brdf = sampleBRDF(s, re, v, l, pdf);
  if(brdf == vec3(0) || pdf < EPSILON){
    return vec3(0);
  }

  float t = dot(l, s.normal);
  vec3 pathThroughput = throughput * (brdf * dot(l, s.normal)) / pdf;
  if(recDepth > infos.minRecursionDepth) {
    float termination = max(c_MinTermination, 1.0 - max(max(pathThroughput.x, pathThroughput.y),pathThroughput.z));
    if(randomFloat(re) < c_MinTermination){
      return vec3(0);
    }
    pathThroughput /= 1.0 - c_MinTermination;
  }
  throughput = pathThroughput;

  traceRayEXT(tlas, rayFlags, cullMask, 0, 0, 0, pos, tmin, l, tmax, 1);

	//TODO: better firefly suppression (see nvpro samples for a good one)
  return nextEventEsitmation(rayPayload.position, v, rayPayload.si, throughput, re) + rayPayload.si.emissiveColor * throughput;
}

#endif //LIGHTING_H
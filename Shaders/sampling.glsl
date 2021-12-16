#ifndef SAMPLING_H
#define SAMPLING_H

#include "ptConstants.glsl"
#include "math.glsl"
#include "random.glsl"
#include "color.glsl"
#include "brdf.glsl"

// sampling half-angle directions from GGX normal distribution
vec3 sampleGGX(vec2 r, float alpha2){
    float phi = 2 * PI * r.x;
    float cosH = sqrt((1.0-r.y) / (1.0 + (alpha2 - 1.0f) * r.y));
    float sinH = sqrt(1.0 - pow2(cosH));
    return vec3(sinH * cos(phi), sinH * sin(phi), cosH);
}

vec3 sampleHemisphere(vec2 r){
    float t = 2 * PI * r.y;
    vec2 d = sqrt(r.x) * vec2(cos(t), sin(t));
    return vec3(d, sqrt(max(0,1.0 - pow2(d.x) - pow2(d.y))));
}

vec2 sampleTriangle(vec2 u){
    float uxsqrt = sqrt(u.x);
    return vec2(1.0f - uxsqrt, u.y * uxsqrt);
}

vec3 sampleBRDF(SurfaceInfo s, RandomEngine re, vec3 v,out vec3 l,out float pdf){
    vec3 h;
    vec3 u = randomVec3(re);
    float specularSW = specularSampleWeight(s);

    //sample specular or diffuse reflection based on sampling weight
    if(u.z < specularSW){
        h = sampleGGX(u.xy, pow2(s.alphaRoughness));
        h = s.basis * h;
        l = -reflect(v, h);
    }
    else{
        l = sampleHemisphere(u.xy);
        l = s.basis * l;
        h = normalize(v + l);
    }
    pdf = pdfBRDF(s, v, l, h);
    return BRDF(v, l, h, s);
}

#endif //SAMPLING_H
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
    mat3 basis = s.basis;
    bool entering = dot(v, s.normal) >= 0;
    if(s.illuminationType == 7 && !entering) basis[2] *= -1;

    //sample specular or diffuse reflection based on sampling weight
    if(u.z < specularSW){
        h = sampleGGX(u.xy, pow2(s.alphaRoughness));
        h = basis * h;
        l = -reflect(v, h);
    }
    else{
        l = sampleHemisphere(u.xy);
        l = basis * l;
        h = normalize(v + l);
    }

    pdf = pdfBRDF(s, v, l, h);
    vec3 brdf = BRDF(v, l, h, s);
    //decide if material should refract
    if(s.illuminationType == 7){
        float f = specularReflection(vec3(1), vec3(0), dot(v, h)).x;
        if(randomFloat(re) < f || !entering){    //refracting
            float t = entering ? s.indexOfRefraction : 1 / s.indexOfRefraction; // it is assumed that only air is anohter medium that is participating
            vec3 diff = (dot(basis[2], l)) * basis[2];
            l -= diff + diff  * t;
            l = normalize(l);
            
            brdf = s.transmissiveColor;
            pdf = 1;
        }
        else{
            pdf = 1;
            brdf = vec3(1);
        }
    }
    return brdf;
}

#endif //SAMPLING_H
#ifndef GENERAL
#define GENERAL

const float PI = 3.14159265359;
const float RECIPROCAL_PI = 0.31830988618;
const float RECIPROCAL_PI2 = 0.15915494;
const float EPSILON = 1e-6;
const float BLEND_ALPHA = .2f;
const float c_MinRoughness = 0.04;
const float c_MaxRadiance = 1e1;
const float c_MinTermination = 0.05;

float pow2(float f){
    return f * f;
}

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
  //float shininess;
  //float ior;       // index of refraction
  //float dissolve;  // 1 == opaque; 0 == fully transparent
  //int   illum;     // illumination model (see http://www.fileformat.info/format/material/)
  //int   textureId;
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
};

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

struct RandomEngine{
    uvec2 state;
};

// Light source types(lst)
const uint lst_undefined = 0;
const uint lst_directional = 1;
const uint lst_point = 2;
const uint lst_spot = 3;
const uint lst_ambient = 4;
const uint lst_area = 5;


// Encapsulate the various inputs used by the various functions in the shading equation
// We store values in this struct to simplify the integration of alternative implementations
// of the shading terms, outlined in the Readme.MD Appendix.
struct SurfaceInfo{
    float perceptualRoughness;    // roughness value, as authored by the model creator (input to shader)
    float metalness;              // metallic value at the surface
    vec3 reflectance0;            // full reflectance color (normal incidence angle)
    vec3 reflectance90;           // reflectance color at grazing angle
    float alphaRoughness;         // roughness mapped to a more linear change in the roughness (proposed by [2])
    vec3 diffuseColor;            // color contribution from diffuse lighting
    vec3 specularColor;           // color contribution from specular lighting
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
#ifdef VERTEXINFOAVAILABLE
Vertex unpack(uint index, uint objId){
    Vertex v;
    v.pos.x = pos[nonuniformEXT(objId)].p[3 * index];
    v.pos.y = pos[nonuniformEXT(objId)].p[3 * index + 1];
    v.pos.z = pos[nonuniformEXT(objId)].p[3 * index + 2];
    v.normal.x = nor[nonuniformEXT(objId)].n[3 * index];
    v.normal.y = nor[nonuniformEXT(objId)].n[3 * index + 1];
    v.normal.z = nor[nonuniformEXT(objId)].n[3 * index + 2];
    v.uv.x = tex[nonuniformEXT(objId)].t[2 * index];
    v.uv.y = tex[nonuniformEXT(objId)].t[2 * index + 1];

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
    m.alphaCutoff = p.emissionTextureId.w;
    return m;
};

float luminance(vec3 color)
{
	return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

//tangent bitangent calculation as in https://wiki.delphigl.com/index.php/TBN_Matrix
vec3 getTangent(vec3 A, vec3 B, vec3 C,  vec2 Auv, vec2 Buv, vec2 Cuv)
{
  float Bv_Cv = Buv.y - Cuv.y;
  if(Bv_Cv == 0.0)
    return (B-C)/(Buv.x-Cuv.x);
  
  float Quotient = (Auv.y - Cuv.y)/(Bv_Cv);
  vec3 D   = C   + (B  -C)   * Quotient;
  vec2 Duv = Cuv + (Buv-Cuv) * Quotient;
  return (D-A)/(Duv.x - Auv.x);
}
vec3 getBitangent(vec3 A, vec3 B, vec3 C,  vec2 Auv, vec2 Buv, vec2 Cuv)
{
  return getTangent(A, C, B,  Auv.yx, Cuv.yx, Buv.yx);
}
vec3 project(vec3 a, vec3 b){
	return dot(a,b)/dot(b,b)*(b);
}
//returns TBN matrix orthonormalized, N is not perturbed
mat3 gramSchmidt(vec3 T, vec3 B, vec3 N){
  vec3 t = T - project(T,N);
  vec3 b = B - project(B, t) - project(B, N);
  return mat3(normalize(t), normalize(b), N);
}

vec4 SRGBtoLINEAR(vec4 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(2.2));
    return vec4(linOut,srgbIn.w);
}
vec3 SRGBtoLINEAR(vec3 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(2.2));
    return linOut;
}

vec4 LINEARtoSRGB(vec4 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(1.0 / 2.2));
    return vec4(linOut, srgbIn.w);
}

vec3 LINEARtoSRGB(vec3 srgbIn)
{
    vec3 linOut = pow(srgbIn.xyz, vec3(1.0 / 2.2));
    return linOut;
}

float rcp(const in float value)
{
    return 1.0 / value;
}

float pow5(const in float value)
{
    return value * value * value * value * value;
}

#ifdef TEXTURESAVAILABLE
// Find the normal for this fragment, pulling either from a predefined normal map
// or from the interpolated mesh normal and tangent attributes.
vec3 getNormal(mat3 TBN, sampler2D normalMap, vec2 uv)
{
  if(textureSize(normalMap, 0) == ivec2(1,1)) 
    return TBN[2];
  // Perturb normal, see http://www.thetenthplanet.de/archives/1180
  vec3 tangentNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;
  
  return normalize(TBN * tangentNormal);
}

#endif

// Basic Lambertian diffuse
// Implementation from Lambert's Photometria https://archive.org/details/lambertsphotome00lambgoog
// See also [1], Equation 1
vec3 BRDF_Diffuse_Lambert(PBRInfo pbrInputs)
{
    return pbrInputs.s.diffuseColor * RECIPROCAL_PI;
}

vec3 BRDF_Diffuse_Custom_Lambert(PBRInfo pbrInputs)
{
    return pbrInputs.s.diffuseColor * RECIPROCAL_PI * pow(pbrInputs.NdotV, 0.5 + 0.3 * pbrInputs.s.perceptualRoughness);
}

// [Gotanda 2012, "Beyond a Simple Physically Based Blinn-Phong Model in Real-Time"]
vec3 BRDF_Diffuse_OrenNayar(PBRInfo pbrInputs)
{
    float a = pbrInputs.s.alphaRoughness;
    float s = a;// / ( 1.29 + 0.5 * a );
    float s2 = s * s;
    float VoL = 2 * pbrInputs.VdotH * pbrInputs.VdotH - 1;		// double angle identity
    float Cosri = pbrInputs.VdotL - pbrInputs.NdotV * pbrInputs.NdotL;
    float C1 = 1 - 0.5 * s2 / (s2 + 0.33);
    float C2 = 0.45 * s2 / (s2 + 0.09) * Cosri * ( Cosri >= 0 ? 1.0 / max(pbrInputs.NdotL, pbrInputs.NdotV) : 1 );
    return pbrInputs.s.diffuseColor / PI * ( C1 + C2 ) * ( 1 + pbrInputs.s.perceptualRoughness * 0.5 );
}

// [Gotanda 2014, "Designing Reflectance Models for New Consoles"]
vec3 BRDF_Diffuse_Gotanda(PBRInfo pbrInputs)
{
    float a = pbrInputs.s.alphaRoughness;
    float a2 = a * a;
    float F0 = 0.04;
    float VoL = 2 * pbrInputs.VdotH * pbrInputs.VdotH - 1;		// double angle identity
    float Cosri = VoL - pbrInputs.NdotV * pbrInputs.NdotL;
    float a2_13 = a2 + 1.36053;
    float Fr = ( 1 - ( 0.542026*a2 + 0.303573*a ) / a2_13 ) * ( 1 - pow( 1 - pbrInputs.NdotV, 5 - 4*a2 ) / a2_13 ) * ( ( -0.733996*a2*a + 1.50912*a2 - 1.16402*a ) * pow( 1 - pbrInputs.NdotV, 1 + rcp(39*a2*a2+1) ) + 1 );
    //float Fr = ( 1 - 0.36 * a ) * ( 1 - pow( 1 - NoV, 5 - 4*a2 ) / a2_13 ) * ( -2.5 * Roughness * ( 1 - NoV ) + 1 );
    float Lm = ( max( 1 - 2*a, 0 ) * ( 1 - pow5( 1 - pbrInputs.NdotL ) ) + min( 2*a, 1 ) ) * ( 1 - 0.5*a * (pbrInputs.NdotL - 1) ) * pbrInputs.NdotL;
    float Vd = ( a2 / ( (a2 + 0.09) * (1.31072 + 0.995584 * pbrInputs.NdotV) ) ) * ( 1 - pow( 1 - pbrInputs.NdotL, ( 1 - 0.3726732 * pbrInputs.NdotV * pbrInputs.NdotV ) / ( 0.188566 + 0.38841 * pbrInputs.NdotV ) ) );
    float Bp = Cosri < 0 ? 1.4 * pbrInputs.NdotV * pbrInputs.NdotL * Cosri : Cosri;
    float Lr = (21.0 / 20.0) * (1 - F0) * ( Fr * Lm + Vd + Bp );
    return pbrInputs.s.diffuseColor * RECIPROCAL_PI * Lr;
}

vec3 BRDF_Diffuse_Burley(PBRInfo pbrInputs)
{
    float energyBias = mix(pbrInputs.s.perceptualRoughness, 0.0, 0.5);
    float energyFactor = mix(pbrInputs.s.perceptualRoughness, 1.0, 1.0 / 1.51);
    float fd90 = energyBias + 2.0 * pbrInputs.VdotH * pbrInputs.VdotH * pbrInputs.s.perceptualRoughness;
    float f0 = 1.0;
    float lightScatter = f0 + (fd90 - f0) * pow(1.0 - pbrInputs.NdotL, 5.0);
    float viewScatter = f0 + (fd90 - f0) * pow(1.0 - pbrInputs.NdotV, 5.0);

    return pbrInputs.s.diffuseColor * lightScatter * viewScatter * energyFactor;
}

vec3 BRDF_Diffuse_Disney(PBRInfo pbrInputs)
{
	float Fd90 = 0.5 + 2.0 * pbrInputs.s.perceptualRoughness * pbrInputs.VdotH * pbrInputs.VdotH;
    vec3 f0 = vec3(0.1);
	vec3 invF0 = vec3(1.0, 1.0, 1.0) - f0;
	float dim = min(invF0.r, min(invF0.g, invF0.b));
	float result = ((1.0 + (Fd90 - 1.0) * pow(1.0 - pbrInputs.NdotL, 5.0 )) * (1.0 + (Fd90 - 1.0) * pow(1.0 - pbrInputs.NdotV, 5.0 ))) * dim;
	return pbrInputs.s.diffuseColor * result;
}

float specularSampleWeight(SurfaceInfo s){
    float wD = mix(luminance(s.diffuseColor), 0, s.metalness);
    float wS = luminance(s.specularColor);
    return min(1, wS/(wD + wD));
}

float pdfBRDF(SurfaceInfo s, vec3 v, vec3 l, vec3 h){
    float pdfDiffuse = RECIPROCAL_PI * dot(l, s.normal);        //analytical solution for diffuse lambert
    float alpha2 = pow2(s.alphaRoughness);
    float pdfSpecular = alpha2 / (PI * pow2(pow2(dot(h, s.normal)) * (alpha2 - 1.0f) + 1.0f));  //analytical pdf for GGX normal distribution
    pdfSpecular *= dot(h, s.normal);
    float specularSW = specularSampleWeight(s);
    return mix(pdfDiffuse, pdfSpecular, specularSW);
}

// The following equation models the Fresnel reflectance term of the spec equation (aka F())
// Implementation of fresnel from [4], Equation 15
vec3 specularReflection(PBRInfo pbrInputs)
{
    //return pbrInputs.reflectance0 + (pbrInputs.reflectance90 - pbrInputs.reflectance0) * pow(clamp(1.0 - pbrInputs.VdotH, 0.0, 1.0), 5.0);
    return pbrInputs.s.reflectance0 + (pbrInputs.s.reflectance90 - pbrInputs.s.reflectance90*pbrInputs.s.reflectance0) * exp2((-5.55473 * pbrInputs.VdotH - 6.98316) * pbrInputs.VdotH);
}

// This calculates the specular geometric attenuation (aka G()),
// where rougher material will reflect less light back to the viewer.
// This implementation is based on [1] Equation 4, and we adopt their modifications to
// alphaRoughness as input as originally proposed in [2].
float geometricOcclusion(PBRInfo pbrInputs)
{
    float NdotL = pbrInputs.NdotL;
    float NdotV = pbrInputs.NdotV;
    float r = pbrInputs.s.alphaRoughness * pbrInputs.s.alphaRoughness;

    float attenuationL = 2.0 * NdotL / (NdotL + sqrt(r + (1.0 - r) * (NdotL * NdotL)));
    float attenuationV = 2.0 * NdotV / (NdotV + sqrt(r + (1.0 - r) * (NdotV * NdotV)));
    return attenuationL * attenuationV;
}

// The following equation(s) model the distribution of microfacet normals across the area being drawn (aka D())
// Implementation from "Average Irregularity Representation of a Roughened Surface for Ray Reflection" by T. S. Trowbridge, and K. P. Reitz
// Follows the distribution function recommended in the SIGGRAPH 2013 course notes from EPIC Games [1], Equation 3.
float microfacetDistribution(PBRInfo pbrInputs)
{
    float roughnessSq = pbrInputs.s.alphaRoughness * pbrInputs.s.alphaRoughness;
    float f = (pbrInputs.NdotH * roughnessSq - pbrInputs.NdotH) * pbrInputs.NdotH + 1.0;
    return roughnessSq / (PI * f * f);
}

vec3 BRDF(vec3 v, vec3 l, vec3 h, SurfaceInfo s)
{
    vec3 n = s.normal;

    float NdotL = clamp(dot(n, l), 0.001, 1.0);
    float NdotV = clamp(abs(dot(n, v)), 0.001, 1.0);
    float NdotH = clamp(dot(n, h), 0.0, 1.0);
    float LdotH = clamp(dot(l, h), 0.0, 1.0);
    float VdotH = clamp(dot(v, h), 0.0, 1.0);
    float VdotL = clamp(dot(v, l), 0.0, 1.0);

    PBRInfo pbrInputs = PBRInfo(NdotL,
                                NdotV,
                                NdotH,
                                LdotH,
                                VdotH,
                                VdotL,
                                s);

    // Calculate the shading terms for the microfacet specular shading model
    vec3 F = specularReflection(pbrInputs);
    float G = geometricOcclusion(pbrInputs);
    float D = microfacetDistribution(pbrInputs);

    const vec3 u_LightColor = vec3(1.0);

    // Calculation of analytical lighting contribution
    vec3 diffuseContrib = (1.0 - F) * BRDF_Diffuse_Disney(pbrInputs);
    vec3 specContrib = F * G * D / (4.0 * NdotL * NdotV);
    // Obtain final intensity as reflectance (BRDF) scaled by the energy of the light (cosine law)
    vec3 color = diffuseContrib + specContrib;

    return color;
}

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
    return vec3(d, sqrt(1.0 - pow2(d.x) - pow2(d.y)));
}

uint rotl(uint x, uint k){
    return (x << k) | (x >> (32 - k));
}

// Xoroshiro64* RNG
uint randomNumber(inout RandomEngine e){
    uint result = e.state.x * 0x9e3779bb;

    e.state.y ^= e.state.x;
    e.state.x = rotl(e.state.x, 26) ^ e.state.y ^ (e.state.y << 9);
    e.state.y = rotl(e.state.y, 13);

    return result;
}

float randomFloat(inout RandomEngine e){
    uint u = 0x3f800000 | (randomNumber(e) >> 9);
    return uintBitsToFloat(u) - 1.0;
}

uint randomUInt(inout RandomEngine e, uint nmax){
    float f = randomFloat(e);
    return uint(floor(f * nmax));
}

vec2 randomVec2(inout RandomEngine e){
    return vec2(randomFloat(e), randomFloat(e));
}

vec3 randomVec3(inout RandomEngine e){
    return vec3(randomFloat(e), randomFloat(e), randomFloat(e));
}

vec2 sampleTriangle(vec2 u){
    float uxsqrt = sqrt(u.x);
    return vec2(1.0f - uxsqrt, u.y * uxsqrt);
}

// Thomas Wang 32-bit hash.
uint wangHash(uint seed){
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

//init random engine
RandomEngine rEInit(uvec2 id, uint frameIndex){
    uint s0 = (id.x << 16) | id.y;
    uint s1 = frameIndex;

    RandomEngine re;
    re.state.x = wangHash(s0);
    re.state.y = wangHash(s1);
    return re;
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

float convertMetallic(vec3 diffuse, vec3 specular, float maxSpecular)
{
    float perceivedDiffuse = sqrt(0.299 * diffuse.r * diffuse.r + 0.587 * diffuse.g * diffuse.g + 0.114 * diffuse.b * diffuse.b);
    float perceivedSpecular = sqrt(0.299 * specular.r * specular.r + 0.587 * specular.g * specular.g + 0.114 * specular.b * specular.b);

    if (perceivedSpecular < c_MinRoughness)
    {
        return 0.0;
    }

    float a = c_MinRoughness;
    float b = perceivedDiffuse * (1.0 - maxSpecular) / (1.0 - c_MinRoughness) + perceivedSpecular - 2.0 * c_MinRoughness;
    float c = c_MinRoughness - perceivedSpecular;
    float D = max(b * b - 4.0 * a * c, 0.0);
    return clamp((-b + sqrt(D)) / (2.0 * a), 0.0, 1.0);
}

struct RayPayload {
	vec3 color;
    vec4 albedo;
    vec3 throughput;
	vec3 position;
    RandomEngine re;
	float reflector;
    SurfaceInfo si;
};

vec3 blerp(vec2 b, vec3 p1, vec3 p2, vec3 p3)
{
    return (1.0 - b.x - b.y) * p1 + b.x * p2 + b.y * p3;
}

float powerHeuristics(float a, float b){
    float f = a * a;
    float g = b * b;
    return f / (f + g);
}

#endif
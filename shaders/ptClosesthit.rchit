#version 460
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable

#include "ptStructures.glsl"
#include "layoutPTGeometry.glsl"
#include "layoutPTGeometryImages.glsl"
#include "layoutPTUniform.glsl"

layout(location = 1) rayPayloadInEXT RayPayload rayPayload;
hitAttributeEXT vec2 attribs;

#include "ptConstants.glsl"
#include "geometry.glsl"
#include "color.glsl"

void main()
{
    const float epsilon = 1e-6;
    ObjectInstance instance = instances.i[gl_InstanceCustomIndexEXT];
    uint objId = int(instance.meshId);
    uint indexStride = int(instances.i[gl_InstanceCustomIndexEXT].indexStride);
    uvec3 index = unpackIndex(objId, gl_PrimitiveID, indexStride);

    Vertex v0 = unpackVertex(index.x, objId);
	Vertex v1 = unpackVertex(index.y, objId);
	Vertex v2 = unpackVertex(index.z, objId);

    const vec3 bar = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    vec2 texCoord = v0.uv * bar.x + v1.uv * bar.y + v2.uv * bar.z;
    vec4 diffuse = SRGBtoLINEAR(texture(diffuseMap[nonuniformEXT(objId)], texCoord));
    diffuse.rgb *= diffuse.a;
    vec3 position = v0.pos * bar.x + v1.pos * bar.y + v2.pos * bar.z;
    position = (instance.objectMat * vec4(position, 1)).xyz;
    vec3 normal = normalize(v0.normal * bar.x + v1.normal * bar.y + v2.normal * bar.z).xyz;//.xzy;
    if(isinf(normal.x) || isnan(normal.x)) normal = vec3(0,1,0);
    mat4 normalObj = transpose(inverse(instance.objectMat));
    normal = normalize((normalObj * vec4(normal, 0)).xyz);
    if(v0.uv == v1.uv) v1.uv += vec2(epsilon,0);
    if(v0.uv == v2.uv) v2.uv += vec2(0,epsilon);
    if(v1.uv == v2.uv) v2.uv += vec2(epsilon);
    vec3 T = (normalObj * vec4(getTangent(v0.pos, v1.pos, v2.pos, v0.uv, v1.uv, v2.uv).xyz, 0)).xyz;
    //T = (instance.objectMat * vec4(T, 0)).xyz;
    vec3 B = (normalObj * vec4(getBitangent(v0.pos, v1.pos, v2.pos, v0.uv, v1.uv, v2.uv).xyz, 0)).xyz;
    //B = (instance.objectMat * vec4(B, 0)).xyz;
    mat3 TBN = gramSchmidt(T, B, normal);
    normal = getNormal(TBN, normalMap[nonuniformEXT(objId)], texCoord);

    WaveFrontMaterial mat = unpackMaterial(materials.m[objId]);
    diffuse.rgb *= mat.diffuse.rgb;
    float perceptualRoughness = 0;

    const vec3 f0 = vec3(.04);

    vec4 specular;
    if(textureSize(specularMap[nonuniformEXT(objId)], 0) == ivec2(1,1))
        specular = vec4(mat.specular, mat.roughness);
    else
        specular = SRGBtoLINEAR(texture(specularMap[nonuniformEXT(objId)], texCoord));
    perceptualRoughness = specular.a;

    float maxSpecular = max(max(specular.r, specular.g), specular.b);

    float metallic = convertMetallic(diffuse.rgb, specular.rgb, maxSpecular);

    vec3 baseColorDiffusePart = diffuse.rgb * ((1.0 - maxSpecular) / (1 - c_MinRoughness) / max(1 - metallic, epsilon));
    vec3 baseColorSpecularPart = specular.rgb - (vec3(c_MinRoughness) * (1 - metallic) * (1 / max(metallic, epsilon)));
    vec4 baseColor = vec4(mix(baseColorDiffusePart, baseColorSpecularPart, metallic * metallic), diffuse.a);

    vec3 diffuseColor = baseColor.rgb * (vec3(1.0) - f0);
    diffuseColor *= 1.0 - metallic;

    float alphaRoughness = perceptualRoughness * perceptualRoughness;
    vec3 specularColor = mix(f0, baseColor.rgb, metallic);

    float reflectance = max(max(specularColor.r, specularColor.g), specularColor.b);

    float reflectance90 = clamp(reflectance * 25, 0, 1);
    vec3 specularEnvironmentR0 = specularColor.rgb;
    vec3 specularEnvironmentR90 = vec3(1) * reflectance90;
    vec3 v = normalize(-gl_WorldRayDirectionEXT);
    //surface emission
    vec3 emissiveColor = mat.emission * SRGBtoLINEAR(texture(emissiveMap[nonuniformEXT(objId)], texCoord)).rgb;
    if(dot(v, normal) < 0) emissiveColor = vec3(0);

    rayPayload.si = SurfaceInfo(perceptualRoughness, metallic, alphaRoughness, mat.illum, specularEnvironmentR0, specularEnvironmentR90, diffuseColor, specularColor, emissiveColor, mat.transmittance, normal, TBN, mat.ior);

    rayPayload.position = position;
	
	rayPayload.category_id = mat.category_id;
}
#ifndef GEOMETRY_H
#define GEOMETRY_H

uvec3 unpackIndex(uint objId, uint primitiveID, uint indexStride){
    uvec3 index;
    if(indexStride == 4)  //full uints are in the indexbuffer
        index = ivec3(ind[nonuniformEXT(objId)].i[3 * primitiveID], ind[nonuniformEXT(objId)].i[3 * primitiveID + 1], ind[nonuniformEXT(objId)].i[3 * primitiveID + 2]);
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
    return index;
}

Vertex unpackVertex(uint index, uint objId){
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

WaveFrontMaterial unpackMaterial(WaveFrontMaterialPacked p){
    WaveFrontMaterial m;
    m.ambient = p.ambientRoughness.xyz;
    m.diffuse = p.diffuseIor.xyz;
    m.specular = p.specularDissolve.xyz;
    m.transmittance = p.transmittanceIllum.xyz;
    m.emission = p.emissionTextureId.xyz;
    m.roughness = p.ambientRoughness.w;
    m.ior = p.diffuseIor.w;
    m.dissolve = p.specularDissolve.w;
    m.illum = int(p.transmittanceIllum.w);
    m.alphaCutoff = p.emissionTextureId.w;
    m.category_id = p.category_id;
    return m;
};

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

vec3 getNormal(mat3 TBN, sampler2D normalMap, vec2 uv)
{
    if(textureSize(normalMap, 0) == ivec2(1,1)) 
        return TBN[2];
    // Perturb normal, see http://www.thetenthplanet.de/archives/1180
    vec3 tangentNormal = texture(normalMap, uv).xyz * 2.0 - 1.0;
  
    return normalize(TBN * tangentNormal);
}

#endif //GEOMETRY_H
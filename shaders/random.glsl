#ifndef RANDOM_H
#define RANDOM_H

// Improvement could be made by using https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-37-efficient-random-number-generation-and-application
// for shorter version see https://math.stackexchange.com/questions/337782/pseudo-random-number-generation-on-the-gpu
//struct RandomEngine{
//    uvec2 state;
//};
struct RandomEngine{
    uvec4 state;
};

uint rotl(uint x, uint k){
    return (x << k) | (x >> (32 - k));
}

uint TausStep(uint z, int S1, int S2, int S3, uint M)
{
    uint b = (((z << S1) ^ z) >> S2);
    return (((z & M) << S3) ^ b);    
}

uint LCGStep(uint z, uint A, uint C)
{
    return (A * z + C);    
}

// Xoroshiro64* RNG
//uint randomNumber(inout RandomEngine e){
//    uint result = e.state.x * 0x9e3779bb;
//
//    e.state.y ^= e.state.x;
//    e.state.x = rotl(e.state.x, 26) ^ e.state.y ^ (e.state.y << 9);
//    e.state.y = rotl(e.state.y, 13);
//
//    return result;
//}

float randomNumber(inout RandomEngine e){
    e.state.x = TausStep(e.state.x, 13, 19, 12, 4294967294);
    e.state.y = TausStep(e.state.y, 2, 25, 4, 4294967288);
    e.state.z = TausStep(e.state.z, 3, 11, 17, 4294967280);
    e.state.w = LCGStep(e.state.w, 1664525, 1013904223);

    float result;
    result = 2.3283064365387e-10 * (e.state.x ^ e.state.y ^ e.state.z ^ e.state.w);

    return result;
}

//float randomFloat(inout RandomEngine e){
//    uint u = 0x3f800000 | (randomNumber(e) >> 9);
//    return uintBitsToFloat(u) - 1.0;
//}

float randomFloat(inout RandomEngine e){
    return randomNumber(e);
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
    uint s0 = id.x;
    uint s1 = id.y;
    uint s2 = frameIndex;
    uint s3 = (s0 + s1) * s2;

    RandomEngine re;
    re.state.x = wangHash(s0);
    re.state.y = wangHash(s1);
    re.state.z = wangHash(s2);
    re.state.w = wangHash(s3);
    return re;
}

#endif //RANDOM_H
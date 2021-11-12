#ifndef RANDOM_H
#define RANDOM_H

// Improvement could be made by using https://developer.nvidia.com/gpugems/gpugems3/part-vi-gpu-computing/chapter-37-efficient-random-number-generation-and-application
// for shorter version see https://math.stackexchange.com/questions/337782/pseudo-random-number-generation-on-the-gpu

struct RandomEngine{
    uvec2 state;
};

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

#endif //RANDOM_H
#ifndef MATH_H
#define MATH_H

float rcp(const in float value)
{
    return 1.0 / value;
}

float pow2(float f){
    return f * f;
}

float pow5(const in float value)
{
    return value * value * value * value * value;
}

vec3 blerp(vec2 b, vec3 p1, vec3 p2, vec3 p3)
{
    return (1.0 - b.x - b.y) * p1 + b.x * p2 + b.y * p3;
}

#endif //MATH_H
#ifndef COLOR_H
#define COLOR_H

float luminance(vec3 color)
{
	return dot(color, vec3(0.2126, 0.7152, 0.0722));
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

#endif //COLOR_H
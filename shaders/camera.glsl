#ifndef CAMERA_H
#define CAMERA_H

#include "sampling.glsl"

void createRay(uvec2 pixelPos, uvec2 imSize, bool antiAlias,inout RandomEngine re, out vec4 pos, out vec4 dir){
    vec2 pixelCenter = vec2(pixelPos.xy) + vec2(.5);
    if(antiAlias)
        pixelCenter += randomVec2(re) - .5;	//can not be done when reprojecting
    const vec2 normalisedPixelCoord = pixelCenter/vec2(imSize.xy);
    vec2 clipSpaceCoord = normalisedPixelCoord * 2.0 - 1.0;

    pos = camParams.inverseViewMatrix[3];
    dir = camParams.inverseProjectionMatrix * vec4(clipSpaceCoord.x, clipSpaceCoord.y, 1, 1) ;
    dir = camParams.inverseViewMatrix * vec4(normalize(dir.xyz), 0) ;
}

#endif //CAMEAR_H
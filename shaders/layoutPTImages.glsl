#ifndef LAYOUTPTIMAGES_H
#define LAYOUTPTIMAGES_H

#ifdef FINAL_IMAGE
layout(binding = 1, rgba32f) uniform image2D outputImage;
#endif

#ifdef GBUFFER
layout(binding = 15, r32f) uniform image2D depthImage;
layout(binding = 16, rg32f) uniform image2D normalImage;
layout(binding = 17, rgba8) uniform image2D materialImage;
layout(binding = 18, rgba8) uniform image2D albedoImage;
#endif

#ifdef DEMOD_ILLUMINATION_FLOAT
layout(binding = 25, rgba32f) uniform image2D illumination;
#endif

#endif //LAYOUTPTIMAGES_H
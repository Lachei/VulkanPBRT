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

#ifdef PREV_GBUFFER
layout(binding = 19) uniform sampler2D prevDepth;
layout(binding = 20) uniform sampler2D prevNormal;
layout(binding = 21, r16f) uniform image2D motion;
layout(binding = 22, r8) uniform image2D sampleCounts;
layout(binding = 24) uniform sampler2D prevSampleCounts;
#endif

#ifdef DEMOD_ILLUMINATION
layout(binding = 23) uniform sampler2D prevOutput;
layout(binding = 25, rgba16f) uniform image2D illumination;
#endif

#ifdef DEMOD_ILLUMINATION_SQUARED
layout(binding = 27, rgba16f) uniform image2D illuminationSquared;
layout(binding = 28) uniform sampler2D prevIlluminationSquared;
#endif

#endif //LAYOUTPTIMAGES_H
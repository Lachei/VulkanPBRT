#pragma once
#include <vsg/all.h>

#include <cstdint>

struct RayTracingPushConstants{
    vsg::mat4 viewInverse;
    vsg::mat4 projInverse;
    vsg::mat4 prevView;
    uint32_t frameNumber;
    uint32_t sampleNumber;
};

enum class DenoisingType
{
    None,
    BMFR,
    BFR,
    SVG
};

enum class DenoisingBlockSize
{
    x8,
    x16,
    x32,
    x64,
    x8x16x32
};
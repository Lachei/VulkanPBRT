#pragma once
#include <vsg/all.h>

#include <cstdint>

struct RayTracingPushConstants
{
    vsg::mat4 view_inverse;
    vsg::mat4 proj_inverse;
    vsg::mat4 prev_view;
    uint32_t frame_number;
    uint32_t sample_number;
};

enum class DenoisingType
{
    NONE,
    BMFR,
    BFR,
    SVG
};

enum class DenoisingBlockSize
{
    X8,
    X16,
    X32,
    X64,
    X8X16X32
};
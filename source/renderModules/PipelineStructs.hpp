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
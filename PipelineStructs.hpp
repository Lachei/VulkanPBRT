#pragma once
#include "Defines.hpp"
#include <vsg/all.h>

struct RayTracingPushConstants{
    vsg::mat4 viewInverse;
    vsg::mat4 projInverse;
    vsg::mat4 prevView;
    uint frameNumber;
    uint steadyCamFrame;
};
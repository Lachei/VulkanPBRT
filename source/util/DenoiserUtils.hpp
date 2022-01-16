#pragma once

#include <vsg/all.h>
#include <renderModules/PipelineStructs.hpp>
#include <buffers/GBuffer.hpp>
#include <buffers/IlluminationBuffer.hpp>
#include <buffers/AccumulationBuffer.hpp>

namespace DenoiserUtils{
    void addDenoiserToCommands(DenoisingType denoisingType, DenoisingBlockSize denoisingSize, vsg::ref_ptr<vsg::Commands>& commands, vsg::CompileTraversal& compile, int width, int height,
        vsg::ref_ptr<vsg::PushConstants> computeConstants ,vsg::ref_ptr<GBuffer>& gBuffer, vsg::ref_ptr<IlluminationBuffer>& illuminationBuffer, vsg::ref_ptr<AccumulationBuffer>& accumulationBuffer, vsg::ref_ptr<vsg::DescriptorImage>& finalDescriptorImage);
}
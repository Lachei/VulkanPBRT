#pragma once

#include <vsg/all.h>
#include <renderModules/PipelineStructs.hpp>
#include <buffers/GBuffer.hpp>
#include <buffers/IlluminationBuffer.hpp>
#include <buffers/AccumulationBuffer.hpp>

namespace vkpbrt
{
void add_denoiser_to_commands(DenoisingType denoising_type, DenoisingBlockSize denoising_size,
    vsg::ref_ptr<vsg::Commands>& commands, vsg::CompileTraversal& compile, int width, int height,
    vsg::ref_ptr<vsg::PushConstants> compute_constants, vsg::ref_ptr<GBuffer>& g_buffer,
    vsg::ref_ptr<IlluminationBuffer>& illumination_buffer, vsg::ref_ptr<AccumulationBuffer>& accumulation_buffer,
    vsg::ref_ptr<vsg::DescriptorImage>& final_descriptor_image);
}
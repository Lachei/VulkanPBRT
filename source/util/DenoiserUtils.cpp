#include <util/DenoiserUtils.hpp>
#include <renderModules/denoisers/BMFR.hpp>
#include <renderModules/denoisers/BFR.hpp>
#include <renderModules/denoisers/BFRBlender.hpp>

namespace vkpbrt
{
void add_denoiser_to_commands(DenoisingType denoising_type, DenoisingBlockSize denoising_size,
    vsg::ref_ptr<vsg::Commands>& commands, vsg::CompileTraversal& compile, int width, int height,
    vsg::ref_ptr<vsg::PushConstants> compute_constants, vsg::ref_ptr<GBuffer>& g_buffer,
    vsg::ref_ptr<IlluminationBuffer>& illumination_buffer, vsg::ref_ptr<AccumulationBuffer>& accumulation_buffer,
    vsg::ref_ptr<vsg::DescriptorImage>& final_descriptor_image)
{
    switch (denoising_type)
    {
    case DenoisingType::NONE:
        break;
    case DenoisingType::BFR:
        switch (denoising_size)
        {
        case DenoisingBlockSize::X8:
            {
                auto bfr8 = BFR::create(width, height, 8, 8, g_buffer, illumination_buffer, accumulation_buffer);
                bfr8->compile(compile.context);
                bfr8->update_image_layouts(compile.context);
                bfr8->add_dispatch_to_command_graph(commands, compute_constants);
                final_descriptor_image = bfr8->get_final_descriptor_image();
                break;
            }
        case DenoisingBlockSize::X16:
            {
                auto bfr16 = BFR::create(width, height, 16, 16, g_buffer, illumination_buffer, accumulation_buffer);
                bfr16->compile(compile.context);
                bfr16->update_image_layouts(compile.context);
                bfr16->add_dispatch_to_command_graph(commands, compute_constants);
                final_descriptor_image = bfr16->get_final_descriptor_image();
                break;
            }
        case DenoisingBlockSize::X32:
            {
                auto bfr32 = BFR::create(width, height, 32, 32, g_buffer, illumination_buffer, accumulation_buffer);
                bfr32->compile(compile.context);
                bfr32->update_image_layouts(compile.context);
                bfr32->add_dispatch_to_command_graph(commands, compute_constants);
                final_descriptor_image = bfr32->get_final_descriptor_image();
                break;
            }
        case DenoisingBlockSize::X8X16X32:
            {
                auto bfr8 = BFR::create(width, height, 8, 8, g_buffer, illumination_buffer, accumulation_buffer);
                auto bfr16 = BFR::create(width, height, 16, 16, g_buffer, illumination_buffer, accumulation_buffer);
                auto bfr32 = BFR::create(width, height, 32, 32, g_buffer, illumination_buffer, accumulation_buffer);
                auto blender = BFRBlender::create(width, height, illumination_buffer->illumination_images[0],
                    illumination_buffer->illumination_images[1], bfr8->get_final_descriptor_image(),
                    bfr16->get_final_descriptor_image(), bfr32->get_final_descriptor_image());
                bfr8->compile(compile.context);
                bfr8->update_image_layouts(compile.context);
                bfr16->compile(compile.context);
                bfr16->update_image_layouts(compile.context);
                bfr32->compile(compile.context);
                bfr32->update_image_layouts(compile.context);
                blender->compile(compile.context);
                blender->update_image_layouts(compile.context);
                bfr8->add_dispatch_to_command_graph(commands, compute_constants);
                bfr16->add_dispatch_to_command_graph(commands, compute_constants);
                bfr32->add_dispatch_to_command_graph(commands, compute_constants);
                blender->add_dispatch_to_command_graph(commands);
                final_descriptor_image = blender->get_final_descriptor_image();
                break;
            }
        }
        break;
    case DenoisingType::BMFR:
        switch (denoising_size)
        {
        case DenoisingBlockSize::X8:
            {
                auto bmfr8 = BMFR::create(width, height, 8, 8, g_buffer, illumination_buffer, accumulation_buffer, 64);
                bmfr8->compile(compile.context);
                bmfr8->update_image_layouts(compile.context);
                bmfr8->add_dispatch_to_command_graph(commands, compute_constants);
                final_descriptor_image = bmfr8->get_final_descriptor_image();
                break;
            }
        case DenoisingBlockSize::X16:
            {
                auto bmfr16 = BMFR::create(width, height, 16, 16, g_buffer, illumination_buffer, accumulation_buffer);
                bmfr16->compile(compile.context);
                bmfr16->update_image_layouts(compile.context);
                bmfr16->add_dispatch_to_command_graph(commands, compute_constants);
                final_descriptor_image = bmfr16->get_final_descriptor_image();
                break;
            }
        case DenoisingBlockSize::X32:
            {
                auto bmfr32 = BMFR::create(width, height, 32, 32, g_buffer, illumination_buffer, accumulation_buffer);
                bmfr32->compile(compile.context);
                bmfr32->update_image_layouts(compile.context);
                bmfr32->add_dispatch_to_command_graph(commands, compute_constants);
                final_descriptor_image = bmfr32->get_final_descriptor_image();
                break;
            }
        case DenoisingBlockSize::X8X16X32:
            auto bmfr8 = BMFR::create(width, height, 8, 8, g_buffer, illumination_buffer, accumulation_buffer, 64);
            auto bmfr16 = BMFR::create(width, height, 16, 16, g_buffer, illumination_buffer, accumulation_buffer);
            auto bmfr32 = BMFR::create(width, height, 32, 32, g_buffer, illumination_buffer, accumulation_buffer);
            auto blender = BFRBlender::create(width, height, illumination_buffer->illumination_images[1],
                illumination_buffer->illumination_images[2], bmfr8->get_final_descriptor_image(),
                bmfr16->get_final_descriptor_image(), bmfr32->get_final_descriptor_image());
            bmfr8->compile(compile.context);
            bmfr8->update_image_layouts(compile.context);
            bmfr16->compile(compile.context);
            bmfr16->update_image_layouts(compile.context);
            bmfr32->compile(compile.context);
            bmfr32->update_image_layouts(compile.context);
            blender->compile(compile.context);
            blender->update_image_layouts(compile.context);
            bmfr8->add_dispatch_to_command_graph(commands, compute_constants);
            bmfr16->add_dispatch_to_command_graph(commands, compute_constants);
            bmfr32->add_dispatch_to_command_graph(commands, compute_constants);
            blender->add_dispatch_to_command_graph(commands);
            final_descriptor_image = blender->get_final_descriptor_image();
            break;
        }
        break;
    case DenoisingType::SVG:
        std::cout << "Not yet implemented" << std::endl;
        break;
    }
}
}  // namespace vkpbrt
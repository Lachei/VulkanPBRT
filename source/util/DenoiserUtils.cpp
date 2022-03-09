#include <util/DenoiserUtils.hpp>
#include <renderModules/denoisers/BMFR.hpp>
#include <renderModules/denoisers/BFR.hpp>
#include <renderModules/denoisers/BFRBlender.hpp>

namespace vkpbrt
{
    void addDenoiserToCommands(DenoisingType denoisingType, DenoisingBlockSize denoisingSize, vsg::ref_ptr<vsg::Commands>& commands, vsg::CompileTraversal& compile, int width, int height,
        vsg::ref_ptr<vsg::PushConstants> computeConstants ,vsg::ref_ptr<GBuffer>& gBuffer, vsg::ref_ptr<IlluminationBuffer>& illuminationBuffer, vsg::ref_ptr<AccumulationBuffer>& accumulationBuffer, vsg::ref_ptr<vsg::DescriptorImage>& finalDescriptorImage) 
    {
        switch (denoisingType)
        {
        case DenoisingType::None:
            break;
        case DenoisingType::BFR:
            switch (denoisingSize)
            {
            case DenoisingBlockSize::x8:
            {
                auto bfr8 = BFR::create(width, height, 8, 8, gBuffer, illuminationBuffer, accumulationBuffer);
                bfr8->compile(compile.context);
                bfr8->update_image_layouts(compile.context);
                bfr8->add_dispatch_to_command_graph(commands, computeConstants);
                finalDescriptorImage = bfr8->get_final_descriptor_image();
                break;
            }
            case DenoisingBlockSize::x16:
            {
                auto bfr16 = BFR::create(width, height, 16, 16, gBuffer, illuminationBuffer, accumulationBuffer);
                bfr16->compile(compile.context);
                bfr16->update_image_layouts(compile.context);
                bfr16->add_dispatch_to_command_graph(commands, computeConstants);
                finalDescriptorImage = bfr16->get_final_descriptor_image();
                break;
            }
            case DenoisingBlockSize::x32:
            {
                auto bfr32 = BFR::create(width, height, 32, 32, gBuffer, illuminationBuffer, accumulationBuffer);
                bfr32->compile(compile.context);
                bfr32->update_image_layouts(compile.context);
                bfr32->add_dispatch_to_command_graph(commands, computeConstants);
                finalDescriptorImage = bfr32->get_final_descriptor_image();
                break;
            }
            case DenoisingBlockSize::x8x16x32:
            {
                auto bfr8 = BFR::create(width, height, 8, 8, gBuffer, illuminationBuffer, accumulationBuffer);
                auto bfr16 = BFR::create(width, height, 16, 16, gBuffer, illuminationBuffer, accumulationBuffer);
                auto bfr32 = BFR::create(width, height, 32, 32, gBuffer, illuminationBuffer, accumulationBuffer);
                auto blender = BFRBlender::create(width, height,
                                                  illuminationBuffer->illumination_images[0], illuminationBuffer->illumination_images[1],
                                                  bfr8->get_final_descriptor_image(), bfr16->get_final_descriptor_image(), bfr32->get_final_descriptor_image());
                bfr8->compile(compile.context);
                bfr8->update_image_layouts(compile.context);
                bfr16->compile(compile.context);
                bfr16->update_image_layouts(compile.context);
                bfr32->compile(compile.context);
                bfr32->update_image_layouts(compile.context);
                blender->compile(compile.context);
                blender->update_image_layouts(compile.context);
                bfr8->add_dispatch_to_command_graph(commands, computeConstants);
                bfr16->add_dispatch_to_command_graph(commands, computeConstants);
                bfr32->add_dispatch_to_command_graph(commands, computeConstants);
                blender->add_dispatch_to_command_graph(commands);
                finalDescriptorImage = blender->get_final_descriptor_image();
                break;
            }
            }
            break;
        case DenoisingType::BMFR:
            switch (denoisingSize)
            {
            case DenoisingBlockSize::x8:
            {
                auto bmfr8 = BMFR::create(width, height, 8, 8, gBuffer, illuminationBuffer, accumulationBuffer, 64);
                bmfr8->compile(compile.context);
                bmfr8->update_image_layouts(compile.context);
                bmfr8->add_dispatch_to_command_graph(commands, computeConstants);
                finalDescriptorImage = bmfr8->get_final_descriptor_image();
                break;
            }
            case DenoisingBlockSize::x16:
            {
                auto bmfr16 = BMFR::create(width, height, 16, 16, gBuffer, illuminationBuffer, accumulationBuffer);
                bmfr16->compile(compile.context);
                bmfr16->update_image_layouts(compile.context);
                bmfr16->add_dispatch_to_command_graph(commands, computeConstants);
                finalDescriptorImage = bmfr16->get_final_descriptor_image();
                break;
            }
            case DenoisingBlockSize::x32:
            {
                auto bmfr32 = BMFR::create(width, height, 32, 32, gBuffer, illuminationBuffer, accumulationBuffer);
                bmfr32->compile(compile.context);
                bmfr32->update_image_layouts(compile.context);
                bmfr32->add_dispatch_to_command_graph(commands, computeConstants);
                finalDescriptorImage = bmfr32->get_final_descriptor_image();
                break;
            }
            case DenoisingBlockSize::x8x16x32:
                auto bmfr8 = BMFR::create(width, height, 8, 8, gBuffer, illuminationBuffer, accumulationBuffer, 64);
                auto bmfr16 = BMFR::create(width, height, 16, 16, gBuffer, illuminationBuffer, accumulationBuffer);
                auto bmfr32 = BMFR::create(width, height, 32, 32, gBuffer, illuminationBuffer, accumulationBuffer);
                auto blender = BFRBlender::create(width, height,
                                                  illuminationBuffer->illumination_images[1], illuminationBuffer->illumination_images[2],
                                                  bmfr8->get_final_descriptor_image(), bmfr16->get_final_descriptor_image(), bmfr32->get_final_descriptor_image());
                bmfr8->compile(compile.context);
                bmfr8->update_image_layouts(compile.context);
                bmfr16->compile(compile.context);
                bmfr16->update_image_layouts(compile.context);
                bmfr32->compile(compile.context);
                bmfr32->update_image_layouts(compile.context);
                blender->compile(compile.context);
                blender->update_image_layouts(compile.context);
                bmfr8->add_dispatch_to_command_graph(commands, computeConstants);
                bmfr16->add_dispatch_to_command_graph(commands, computeConstants);
                bmfr32->add_dispatch_to_command_graph(commands, computeConstants);
                blender->add_dispatch_to_command_graph(commands);
                finalDescriptorImage = blender->get_final_descriptor_image();
                break;
            }
            break;
        case DenoisingType::SVG:
            std::cout << "Not yet implemented" << std::endl;
            break;
        }
    }
}
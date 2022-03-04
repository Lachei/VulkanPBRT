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
                bfr8->updateImageLayouts(compile.context);
                bfr8->addDispatchToCommandGraph(commands, computeConstants);
                finalDescriptorImage = bfr8->getFinalDescriptorImage();
                break;
            }
            case DenoisingBlockSize::x16:
            {
                auto bfr16 = BFR::create(width, height, 16, 16, gBuffer, illuminationBuffer, accumulationBuffer);
                bfr16->compile(compile.context);
                bfr16->updateImageLayouts(compile.context);
                bfr16->addDispatchToCommandGraph(commands, computeConstants);
                finalDescriptorImage = bfr16->getFinalDescriptorImage();
                break;
            }
            case DenoisingBlockSize::x32:
            {
                auto bfr32 = BFR::create(width, height, 32, 32, gBuffer, illuminationBuffer, accumulationBuffer);
                bfr32->compile(compile.context);
                bfr32->updateImageLayouts(compile.context);
                bfr32->addDispatchToCommandGraph(commands, computeConstants);
                finalDescriptorImage = bfr32->getFinalDescriptorImage();
                break;
            }
            case DenoisingBlockSize::x8x16x32:
            {
                auto bfr8 = BFR::create(width, height, 8, 8, gBuffer, illuminationBuffer, accumulationBuffer);
                auto bfr16 = BFR::create(width, height, 16, 16, gBuffer, illuminationBuffer, accumulationBuffer);
                auto bfr32 = BFR::create(width, height, 32, 32, gBuffer, illuminationBuffer, accumulationBuffer);
                auto blender = BFRBlender::create(width, height,
                                                  illuminationBuffer->illuminationImages[0], illuminationBuffer->illuminationImages[1],
                                                  bfr8->getFinalDescriptorImage(), bfr16->getFinalDescriptorImage(), bfr32->getFinalDescriptorImage());
                bfr8->compile(compile.context);
                bfr8->updateImageLayouts(compile.context);
                bfr16->compile(compile.context);
                bfr16->updateImageLayouts(compile.context);
                bfr32->compile(compile.context);
                bfr32->updateImageLayouts(compile.context);
                blender->compile(compile.context);
                blender->updateImageLayouts(compile.context);
                bfr8->addDispatchToCommandGraph(commands, computeConstants);
                bfr16->addDispatchToCommandGraph(commands, computeConstants);
                bfr32->addDispatchToCommandGraph(commands, computeConstants);
                blender->addDispatchToCommandGraph(commands);
                finalDescriptorImage = blender->getFinalDescriptorImage();
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
                bmfr8->updateImageLayouts(compile.context);
                bmfr8->addDispatchToCommandGraph(commands, computeConstants);
                finalDescriptorImage = bmfr8->getFinalDescriptorImage();
                break;
            }
            case DenoisingBlockSize::x16:
            {
                auto bmfr16 = BMFR::create(width, height, 16, 16, gBuffer, illuminationBuffer, accumulationBuffer);
                bmfr16->compile(compile.context);
                bmfr16->updateImageLayouts(compile.context);
                bmfr16->addDispatchToCommandGraph(commands, computeConstants);
                finalDescriptorImage = bmfr16->getFinalDescriptorImage();
                break;
            }
            case DenoisingBlockSize::x32:
            {
                auto bmfr32 = BMFR::create(width, height, 32, 32, gBuffer, illuminationBuffer, accumulationBuffer);
                bmfr32->compile(compile.context);
                bmfr32->updateImageLayouts(compile.context);
                bmfr32->addDispatchToCommandGraph(commands, computeConstants);
                finalDescriptorImage = bmfr32->getFinalDescriptorImage();
                break;
            }
            case DenoisingBlockSize::x8x16x32:
                auto bmfr8 = BMFR::create(width, height, 8, 8, gBuffer, illuminationBuffer, accumulationBuffer, 64);
                auto bmfr16 = BMFR::create(width, height, 16, 16, gBuffer, illuminationBuffer, accumulationBuffer);
                auto bmfr32 = BMFR::create(width, height, 32, 32, gBuffer, illuminationBuffer, accumulationBuffer);
                auto blender = BFRBlender::create(width, height,
                                                  illuminationBuffer->illuminationImages[1], illuminationBuffer->illuminationImages[2],
                                                  bmfr8->getFinalDescriptorImage(), bmfr16->getFinalDescriptorImage(), bmfr32->getFinalDescriptorImage());
                bmfr8->compile(compile.context);
                bmfr8->updateImageLayouts(compile.context);
                bmfr16->compile(compile.context);
                bmfr16->updateImageLayouts(compile.context);
                bmfr32->compile(compile.context);
                bmfr32->updateImageLayouts(compile.context);
                blender->compile(compile.context);
                blender->updateImageLayouts(compile.context);
                bmfr8->addDispatchToCommandGraph(commands, computeConstants);
                bmfr16->addDispatchToCommandGraph(commands, computeConstants);
                bmfr32->addDispatchToCommandGraph(commands, computeConstants);
                blender->addDispatchToCommandGraph(commands);
                finalDescriptorImage = blender->getFinalDescriptorImage();
                break;
            }
            break;
        case DenoisingType::SVG:
            std::cout << "Not yet implemented" << std::endl;
            break;
        }
    }
}
#include <renderModules/FormatConverter.hpp>
#include <vsgXchange/glsl.h>

FormatConverter::FormatConverter(vsg::ref_ptr<vsg::ImageView> srcImage, VkFormat dstFormat, int workWidth, int workHeight):
    width(srcImage->image->extent.width),
    height(srcImage->image->extent.height),
    workWidth(workWidth),
    workHeight(workHeight)
{
    std::vector<std::string> defines;
    switch (dstFormat)
    {
    case VK_FORMAT_B8G8R8A8_UNORM:
        defines.push_back("FORMAT rgba8");
        break;
    default:
        throw vsg::Exception{"FormatConverter::Unknown format"};
    }
    auto options = vsg::Options::create(vsgXchange::glsl::create());
    auto computeStage = vsg::ShaderStage::read(VK_SHADER_STAGE_COMPUTE_BIT, "main", shaderPath, options);
    computeStage->specializationConstants = vsg::ShaderStage::SpecializationConstants{
        {0, vsg::intValue::create(workWidth)}, 
        {1, vsg::intValue::create(workHeight)}
    };
    auto compileHints = vsg::ShaderCompileSettings::create();
    compileHints->vulkanVersion = VK_API_VERSION_1_2;
    compileHints->target = vsg::ShaderCompileSettings::SPIRV_1_4;
    compileHints->defines = defines;
    computeStage->module->hints = compileHints;

    auto bindingMap = computeStage->getDescriptorSetLayoutBindingsMap();
    auto descriptorSetLayout = vsg::DescriptorSetLayout::create(bindingMap.begin()->second.bindings);
    auto pipelineLayout = vsg::PipelineLayout::create(vsg::DescriptorSetLayouts{descriptorSetLayout},
                                                      computeStage->getPushConstantRanges());
    auto pipeline = vsg::ComputePipeline::create(pipelineLayout, computeStage);
    int finalIndex = vsg::ShaderStage::getSetBindingIndex(bindingMap, "final").second;
    auto image = vsg::Image::create();
    image->imageType = VK_IMAGE_TYPE_2D;
    image->format = dstFormat;
    image->extent = srcImage->image->extent;
    image->mipLevels = 1;
    image->arrayLayers = 1;
    image->usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    auto imageView = vsg::ImageView::create(image, VK_IMAGE_ASPECT_COLOR_BIT);
    auto imageInfo = vsg::ImageInfo::create(vsg::ref_ptr<vsg::Sampler>{}, imageView, VK_IMAGE_LAYOUT_GENERAL);
    finalImage = vsg::DescriptorImage::create(imageInfo, finalIndex, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    int sourceIndex = vsg::ShaderStage::getSetBindingIndex(bindingMap, "source").second;
    srcImage->image->usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo = vsg::ImageInfo::create(vsg::Sampler::create(), srcImage, VK_IMAGE_LAYOUT_GENERAL);
    auto srcDscImage = vsg::DescriptorImage::create(imageInfo, sourceIndex, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    auto descriptorSet = vsg::DescriptorSet::create(pipelineLayout->setLayouts[0], vsg::Descriptors{finalImage, srcDscImage });
    bindDescriptorSet = vsg::BindDescriptorSet::create(VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, descriptorSet);

    bindPipeline = vsg::BindComputePipeline::create(pipeline);
}

void FormatConverter::compileImages(vsg::Context &context)
{
    finalImage->compile(context);
}

void FormatConverter::updateImageLayouts(vsg::Context &context)
{
    VkImageSubresourceRange resourceRange{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
    auto finalLayout = vsg::ImageMemoryBarrier::create(VK_ACCESS_NONE_KHR, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL, 0, 0, finalImage->imageInfoList[0]->imageView->image,
        resourceRange);
    auto pipelineBarrier = vsg::PipelineBarrier::create(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_DEPENDENCY_BY_REGION_BIT,
        finalLayout);
    context.commands.push_back(pipelineBarrier);
}

void FormatConverter::addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph)
{
    commandGraph->addChild(bindPipeline);
    commandGraph->addChild(bindDescriptorSet);
    commandGraph->addChild(vsg::Dispatch::create(uint32_t(ceil(float(width) / float(workWidth))), uint32_t(ceil(float(height) / float(workHeight))),
                                                 1));
}

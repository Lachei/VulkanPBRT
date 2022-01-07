#pragma once
#include <vsg/all.h>

class FormatConverter: public vsg::Inherit<vsg::Object, FormatConverter>
{
public:
    FormatConverter(vsg::ref_ptr<vsg::ImageView> srcImage, VkFormat dstFormat, int workWidth = 16, int workHeight = 16);

    void compileImages(vsg::Context& context);
    void updateImageLayouts(vsg::Context& context);
    void addDispatchToCommandGraph(vsg::ref_ptr<vsg::Commands> commandGraph);
    vsg::ref_ptr<vsg::DescriptorImage> finalImage;
private:
    std::string shaderPath = "shaders/formatConverter.comp";
    vsg::ref_ptr<vsg::BindDescriptorSet> bindDescriptorSet;
    vsg::ref_ptr<vsg::BindComputePipeline> bindPipeline;
    int width, height, workWidth, workHeight;
};
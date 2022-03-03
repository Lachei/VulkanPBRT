#pragma once
#include <vsg/all.h>

class FormatConverter : public vsg::Inherit<vsg::Object, FormatConverter>
{
public:
    FormatConverter(
        vsg::ref_ptr<vsg::ImageView> src_image, VkFormat dst_format, int work_width = 16, int work_height = 16);

    void compile_images(vsg::Context& context) const;
    void update_image_layouts(vsg::Context& context) const;
    void add_dispatch_to_command_graph(vsg::ref_ptr<vsg::Commands> command_graph);
    vsg::ref_ptr<vsg::DescriptorImage> final_image;

private:
    std::string _shader_path = "shaders/formatConverter.comp";
    vsg::ref_ptr<vsg::BindDescriptorSet> _bind_descriptor_set;
    vsg::ref_ptr<vsg::BindComputePipeline> _bind_pipeline;
    int _width, _height, _work_width, _work_height;
};
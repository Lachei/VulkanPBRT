#pragma once
#include <vsg/all.h>

#include <cstdint>

enum class IlluminationBufferType
{
    FINAL,
    DEMODULATED,
    FINAL_DEMODULATED,
    FINAL_DIRECT_INDIRECT,
};

// base illumination buffer class
class IlluminationBuffer : public vsg::Inherit<vsg::Object, IlluminationBuffer>
{
public:
    std::vector<std::string> illumination_bindings;
    std::vector<vsg::ref_ptr<vsg::DescriptorImage>> illumination_images;
    uint32_t width, height;

    void compile(vsg::Context& context);

    void update_descriptor(vsg::BindDescriptorSet* desc_set, const vsg::BindingMap& binding_map);

    void update_image_layouts(vsg::Context& context);

    void copy_image(vsg::ref_ptr<vsg::Commands> commands, uint32_t image_index, vsg::ref_ptr<vsg::Image> dst_image);
};

class IlluminationBufferFinal : public vsg::Inherit<IlluminationBuffer, IlluminationBufferFinal>
{
public:
    IlluminationBufferFinal(uint32_t width, uint32_t height);

    void fill_images();
};

class IlluminationBufferFinalDirIndir : public vsg::Inherit<IlluminationBuffer, IlluminationBufferFinalDirIndir>
{
public:
    IlluminationBufferFinalDirIndir(uint32_t width, uint32_t height);

    void fill_images();
};

class IlluminationBufferFinalDemodulated : public vsg::Inherit<IlluminationBuffer, IlluminationBufferFinalDemodulated>
{
public:
    IlluminationBufferFinalDemodulated(uint32_t width, uint32_t height);

    void fill_images();
};

class IlluminationBufferDemodulated : public vsg::Inherit<IlluminationBuffer, IlluminationBufferDemodulated>
{
public:
    IlluminationBufferDemodulated(uint32_t width, uint32_t height);

    void fill_images();
};

class IlluminationBufferDemodulatedFloat : public vsg::Inherit<IlluminationBuffer, IlluminationBufferDemodulatedFloat>
{
public:
    IlluminationBufferDemodulatedFloat(uint32_t width, uint32_t height);

    void fill_images();
};

class IlluminatonBufferFinalFloat : public vsg::Inherit<IlluminationBuffer, IlluminatonBufferFinalFloat>
{
public:
    IlluminatonBufferFinalFloat(uint32_t width, uint32_t height);

    void fill_images();
};
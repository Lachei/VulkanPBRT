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

//base illumination buffer class
class IlluminationBuffer: public vsg::Inherit<vsg::Object, IlluminationBuffer>{
public:
    std::vector<std::string> illuminationBindings;
    std::vector<vsg::ref_ptr<vsg::DescriptorImage>> illuminationImages;
    uint32_t width, height;

    void compile(vsg::Context& context);

    void updateDescriptor(vsg::BindDescriptorSet* descSet, const vsg::BindingMap& bindingMap);

    void updateImageLayouts(vsg::Context& context);

    void copyImage(vsg::ref_ptr<vsg::Commands> commands, uint32_t imageIndex, vsg::ref_ptr<vsg::Image> dstImage);
};

class IlluminationBufferFinal: public vsg::Inherit<IlluminationBuffer, IlluminationBufferFinal>{
public:
    IlluminationBufferFinal(uint32_t width, uint32_t height);;

    void fillImages();
};

class IlluminationBufferFinalDirIndir: public vsg::Inherit<IlluminationBuffer, IlluminationBufferFinalDirIndir>{
public:
    IlluminationBufferFinalDirIndir(uint32_t width, uint32_t height);;

    void fillImages();
};

class IlluminationBufferFinalDemodulated: public vsg::Inherit<IlluminationBuffer, IlluminationBufferFinalDemodulated>{
public:
    IlluminationBufferFinalDemodulated(uint32_t width, uint32_t height);;

    void fillImages();
};

class IlluminationBufferDemodulated: public vsg::Inherit<IlluminationBuffer, IlluminationBufferDemodulated>{
public:
    IlluminationBufferDemodulated(uint32_t width, uint32_t height);;

    void fillImages();
};

class IlluminationBufferDemodulatedFloat: public vsg::Inherit<IlluminationBuffer, IlluminationBufferDemodulatedFloat>{
public:
    IlluminationBufferDemodulatedFloat(uint32_t width, uint32_t height);;

    void fillImages();
};

class IlluminationBufferFinalFloat: public vsg::Inherit<IlluminationBuffer, IlluminationBufferFinalFloat>{
public:
    IlluminationBufferFinalFloat(uint32_t width, uint32_t height);

    void fillImages();
};
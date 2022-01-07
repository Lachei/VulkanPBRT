#pragma once
#include <vsg/all.h>

#include <cstdint>

// class holding all references for the gbuffer
// supports automatically updating the ray tracing descriptor set
// care to update also the RayTracingVisitor.hpp descriptor layout if you add things to the gbuffer
class GBuffer: public vsg::Inherit<vsg::Object, GBuffer>{
public:
    GBuffer(uint32_t width, uint32_t height);

    uint32_t width, height;
    vsg::ref_ptr<vsg::DescriptorImage> depth, normal, material, albedo;

    void updateDescriptor(vsg::BindDescriptorSet* descSet, const vsg::BindingMap& bindingMap);

    void updateImageLayouts(vsg::Context& context);

    void compile(vsg::Context& context);

protected:
    vsg::ref_ptr<vsg::Sampler> sampler;
    void setupImages();
};

#pragma once
#include "ResourceBase.h"
#include <vsg/all.h>

namespace vkpbrt
{
class TextureResource : public ResourceBase
{
protected:
    void load_internal() override;

private:
    vsg::ref_ptr<vsg::Sampler> _sampler;
    vsg::ref_ptr<vsg::Data> _texture_data;
};
}  // namespace vkpbrt

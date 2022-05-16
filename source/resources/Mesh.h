#pragma once
#include "ResourceBase.h"
#include <vsg/all.h>

namespace vkpbrt
{
class MeshResource : public ResourceBase
{
public:
    using ResourceBase::ResourceBase;
    MeshResource(vsg::ref_ptr<vsg::vec3Array> vertices, vsg::ref_ptr<vsg::vec3Array> normals,
        vsg::ref_ptr<vsg::vec2Array> texture_coordinates);

protected:
    void load_internal() override;

private:
    vsg::ref_ptr<vsg::vec3Array> _vertices;
    vsg::ref_ptr<vsg::vec3Array> _normals;
    vsg::ref_ptr<vsg::vec2Array> _texture_coordinates;
};
}  // namespace vkpbrt

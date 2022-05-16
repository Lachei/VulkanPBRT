#include "Mesh.h"

using namespace vkpbrt;

MeshResource::MeshResource(vsg::ref_ptr<vsg::vec3Array> vertices, vsg::ref_ptr<vsg::vec3Array> normals,
    vsg::ref_ptr<vsg::vec2Array> texture_coordinates)
    : ResourceBase(""), _vertices(vertices), _normals(normals), _texture_coordinates(texture_coordinates)
{
    _is_loaded = true;
}

void MeshResource::load_internal()
{
    // TODO: load mesh from _resource_path
}
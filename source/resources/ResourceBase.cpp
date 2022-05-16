#include "ResourceBase.h"

#include <utility>

using namespace vkpbrt;

ResourceBase::ResourceBase(std::filesystem::path resource_file_path)
    : _resource_file_path(std::move(resource_file_path)), _is_loaded(false)
{
}
void ResourceBase::load()
{
    load_internal();
    _is_loaded = true;
}
bool ResourceBase::is_loaded() const
{
    return _is_loaded;
}

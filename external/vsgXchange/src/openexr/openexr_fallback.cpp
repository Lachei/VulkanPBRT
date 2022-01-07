#include <vsgXchange/images.h>

using namespace vsgXchange;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// openEXR ReaderWriter fallback
//
openexr::openexr() :
{
}

vsg::ref_ptr<vsg::Object> openexr::read(const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    return {};
}

vsg::ref_ptr<vsg::Object> openexr::read(std::istream& fin, vsg::ref_ptr<const vsg::Options> options) const
{
    return {};
}

vsg::ref_ptr<vsg::Object> openexr::read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options) const
{
    return {};
}

bool openexr::write(const vsg::Object* object, const vsg::Path& filename, vsg::ref_ptr<const vsg::Options> options) const
{
    return false;
}

bool openexr::write(const vsg::Object* object, std::ostream& fout, vsg::ref_ptr<const vsg::Options> options) const
{
    return false;
}

bool openexr::getFeatures(Features& features) const
{
    return false;
}

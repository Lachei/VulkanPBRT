#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2021 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shimages be included in images
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/io/ReaderWriter.h>

#include <memory>

namespace vsgXchange
{
    /// Composite ReaderWriter that holds the uses load 3rd party images formats.
    /// By defalt utilizes the stbi, dds and ktx ReaderWriters so that users only need to create vsgXchange::images::create() to utilize them all.
    class models : public vsg::Inherit<vsg::CompositeReaderWriter, models>
    {
    public:
        models();
    };

    /// optional assimp ReaderWriter
    class assimp : public vsg::Inherit<vsg::ReaderWriter, assimp>
    {
    public:
        assimp();

        vsg::ref_ptr<vsg::Object> read(const vsg::Path&, vsg::ref_ptr<const vsg::Options>) const;
        vsg::ref_ptr<vsg::Object> read(std::istream&, vsg::ref_ptr<const vsg::Options>) const;
        vsg::ref_ptr<vsg::Object> read(const uint8_t* ptr, size_t size, vsg::ref_ptr<const vsg::Options> options = {}) const override;

        bool getFeatures(Features& features) const override;

    protected:
        class Implementation;
        std::unique_ptr<Implementation> _implementation;
    };
} // namespace vsgXchange

EVSG_type_name(vsgXchange::models);
EVSG_type_name(vsgXchange::assimp);
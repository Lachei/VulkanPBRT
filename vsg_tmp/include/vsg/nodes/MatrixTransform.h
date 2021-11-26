#pragma once

/* <editor-fold desc="MIT License">

Copyright(c) 2018 Robert Osfield

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

</editor-fold> */

#include <vsg/nodes/Group.h>

namespace vsg
{

    class VSG_DECLSPEC MatrixTransform : public Inherit<Group, MatrixTransform>
    {
    public:
        MatrixTransform(Allocator* allocator = nullptr);
        MatrixTransform(const dmat4& matrix, Allocator* allocator = nullptr);

        void read(Input& input) override;
        void write(Output& output) const override;

        void setMatrix(const dmat4& matrix) { _matrix = matrix; }
        dmat4& getMatrix() { return _matrix; }
        const dmat4& getMatrix() const { return _matrix; }

        void setSubgraphRequiresLocalFrustum(bool flag) { _subgraphRequiresLocalFrustum = flag; }
        bool getSubgraphRequiresLocalFrustum() const { return _subgraphRequiresLocalFrustum; }

    protected:
        dmat4 _matrix;
        bool _subgraphRequiresLocalFrustum;
    };
    VSG_type_name(vsg::MatrixTransform);

} // namespace vsg

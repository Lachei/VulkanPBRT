#pragma once

#include <vsg/all.h>

class CountTrianglesVisitor : public vsg::Visitor
{
public:
    CountTrianglesVisitor():triangleCount(0){};

    void apply(vsg::Object& object){
        object.traverse(*this);
    };
    void apply(vsg::Geometry& geometry){
        triangleCount += geometry.indices->data->valueCount() / 3;
    };

    void apply(vsg::VertexIndexDraw& vid)
    {
        triangleCount += vid.indices->data->valueCount() / 3;
    }

    int triangleCount;
};
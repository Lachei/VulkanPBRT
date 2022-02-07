#pragma once

#include <vsg/all.h>

class CountTrianglesVisitor : public vsg::Visitor
{
public:
    CountTrianglesVisitor() : triangle_count(0){};

    void apply(Object& object) override { object.traverse(*this); };
    void apply(vsg::Geometry& geometry) override { triangle_count += geometry.indices->data->valueCount() / 3; };

    void apply(vsg::VertexIndexDraw& vid) override { triangle_count += vid.indices->data->valueCount() / 3; }

    int triangle_count;
};

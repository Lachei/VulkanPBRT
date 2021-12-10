#pragma once

#include <vsg/nodes/Node.h>

#include <vsg/maths/vec3.h>

#include <vsg/traversals/CompileTraversal.h>

namespace vsg
{
    enum LightSourceType
    {
        Undefined,
        Directional,
        Point,
        Spot,
        Ambient,
        Area
    };
    class VSG_DECLSPEC Light : public Inherit<Node, Light>
    {
    public:
        struct PackedLight{
            vec3 v0;
            float type;
            vec3 v1;
            float radius;   //radius is used to give a point light a spherical influence
            vec3 v2;
            float angle;
            vec3 dir;
            float angle2;
            vec4 colorAmbient;
            vec4 colorDiffuse;
            vec4 colorSpecular;
            vec3 strengths; //Attenuation factors, x is constant, y is linear, and z is quadratic.(d is distance) Att = 1/( x + y * d + z * d*d)
            float inclusiveStrength;
        };
        Light(){};
        Light(vsg::ref_ptr<vsg::Light>& other){
            *this = *other;
        }
        std::string name;
        // positions of the vertices of a triangle (only first vertex is used if litght type is not area)
        vec3 v0, v1, v2, dir, colorAmbient, colorDiffuse, colorSpecular, strengths;
        LightSourceType type;
        float angle, angle2, radius;

        PackedLight getPacked() const{
            PackedLight p;
            p.v0 = v0;
            p.v1 = v1;
            p.v2 = v2;
            p.dir = dir;
            p.colorAmbient.set(colorAmbient.x, colorAmbient.y, colorAmbient.z, 0);
            p.colorDiffuse.set(colorDiffuse.x, colorDiffuse.y, colorDiffuse.z, 0);
            p.colorSpecular.set(colorSpecular.x, colorSpecular.y, colorSpecular.z, 0);
            p.type = (float) type;
            p.strengths = strengths;
            p.angle = angle;
            p.angle = angle2;
            p.radius = radius;

            return p;
        }
    protected:
    }; 
}
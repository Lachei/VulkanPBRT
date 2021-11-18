#ifndef LAYOUTPTGEOMETRYIMAGES_H
#define LAYOUTPTGEOMETRYIMAGES_H

layout(binding = 6) uniform sampler2D diffuseMap[];
layout(binding = 7) uniform sampler2D mrMap[];
layout(binding = 8) uniform sampler2D normalMap[];
layout(binding = 10) uniform sampler2D emissiveMap[];
layout(binding = 11) uniform sampler2D specularMap[];

#endif //LAYOUTPTGEOMETRYIMAGES_H
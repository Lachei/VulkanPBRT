#ifndef LAYOUTPTPUSHCONSTANTS_H 
#define LAYOUTPTPUSHCONSTANTS_H

layout(push_constant) uniform PushConstants 
{
	mat4 inverseViewMatrix;
	mat4 inverseProjectionMatrix;
	mat4 prevView;
    mat4 worldToViewNormals;
	uint frameNumber;
	uint sampleNumber;
} camParams;

#endif //LAYOUTPTPUSHCONSTANTS_H
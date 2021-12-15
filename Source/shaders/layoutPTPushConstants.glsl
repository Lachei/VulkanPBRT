#ifndef LAYOUTPTPUSHCONSTANTS_H 
#define LAYOUTPTPUSHCONSTANTS_H

layout(push_constant) uniform PushConstants 
{
	mat4 inverseViewMatrix;
	mat4 inverseProjectionMatrix;
	mat4 prevView;
	uint frameNumber;
	uint sampleNumber;
} camParams;

#endif //LAYOUTPTPUSHCONSTANTS_H
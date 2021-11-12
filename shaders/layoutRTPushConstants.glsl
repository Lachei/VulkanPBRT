#ifndef LAYOUTRTPUSHCONSTANTS_H 
#define LAYOUTRTPUSHCONSTANTS_H

layout(push_constant) uniform PushConstants 
{
	mat4 inverseViewMatrix;
	mat4 inverseProjectionMatrix;
	mat4 prevView;
	uint frameNumber;
	uint sampleNumber;
} camParams;

#endif //LAYOUTRTPUSHCONSTANTS_H
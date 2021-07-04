// Helper defines used in IN_ACCESS define
#define POSITION_LIMIT .1f
#define NORMAL_LIMIT .1f
#define BLOCK_EDGE_HALF (BLOCK_EDGE_LENGTH / 2)
#define HORIZONTAL_BLOCKS (WORKSET_WIDTH / BLOCK_EDGE_LENGTH)
#define BLOCK_INDEX_X (group_id % (HORIZONTAL_BLOCKS + 1))
#define BLOCK_INDEX_Y (group_id / (HORIZONTAL_BLOCKS + 1))
#define IN_BLOCK_INDEX (BLOCK_INDEX_Y * (HORIZONTAL_BLOCKS + 1) + BLOCK_INDEX_X)
#define FEATURE_START (feature_buffer * BLOCK_PIXELS)
#define IN_ACCESS (IN_BLOCK_INDEX * buffers * BLOCK_PIXELS + \
   FEATURE_START + sub_vector * 256 + id)

#define UINT_MAX   4294967295U
#define NOISE_AMOUNT 1e-2
#define BLEND_ALPHA 0.2f
#define SECOND_BLEND_ALPHA 0.1f
#define TAA_BLEND_ALPHA 0.2f


#define BLOCK_EDGE_LENGTH 32
#define BLOCK_PIXELS (BLOCK_EDGE_LENGTH * BLOCK_EDGE_LENGTH)
#define LOCAL_SIZE 256
#define BUFFER_COUNT 13
#define R_EDGE BUFFER_COUNT - 2

#if COMPRESSED_R
#define R_SIZE (R_EDGE * (R_EDGE + 1) / 2)
#define R_ROW_START (R_SIZE - (R_EDGE - y) * (R_EDGE - y + 1) / 2)
#define R_ACCESS (R_ROW_START + x - y)
// Reduces unused values in the begining of each row
// 00 01 02 03 04 05
// 11 12 13 14 15 22
// 23 24 25 33 34 35
// 44 45 55
#else
#define R_ACCESS (x * R_EDGE + y)
// Here - means unused value
// Note: "unused" values are still set to 0 so some operations can be done to
// every element in a row or column
//    0  1  2  3  4  5 x
// 0 00 01 02 03 04 05
// 1  - 11 12 13 14 15
// 2  -  - 22 23 24 25
// 3  -  -  - 33 34 35
// 4  -  -  -  - 44 45
// 5  -  -  -  -  - 55
// y
#endif

shared float reduction[8];

void parallel_reduction_sum(inout float result, inout float var) {
	float t = subgroupAdd(var);             //Adding across the subgroup
	if (subgroupElect()) {
		reduction[gl_SubgroupID] = t;
	}
	memoryBarrierShared();
	if (gl_LocalInvocationID == uvec3(0,0,0)) {
        for (int i = 1; i < gl_NumSubgroups; ++i) {
            var += reduction[i];
        }
        reduction[0] = var;
	}
	memoryBarrierShared();
    var = reduction[0];
}

void parallel_reduction_min(inout float result, inout float var) {
	float t = subgroupMin(var);             //Min across the subgroup
	if (subgroupElect()) {
        reduction[gl_SubgroupID] = t;
	}
	memoryBarrierShared();
	if (gl_LocalInvocationID == uvec3(0, 0, 0)) {
        for (int i = 1; i < gl_NumSubgroups; ++i) {
            var = min(reduction[i],var);
        }
        reduction[0] = var;
	}
	memoryBarrierShared();
    var = reduction[0];
}

void parallel_reduction_max(inout float result, inout float var) {
	float t = subgroupMax(var);             //Max across the subgroup
	if (subgroupElect()) {
        reduction[gl_SubgroupID] = t;
	}
	memoryBarrierShared();
	if (gl_LocalInvocationID == uvec3(0, 0, 0)) {
        for (int i = 1; i < gl_NumSubgroups; ++i) {
            var = max(reduction[i], var);
        }
        reduction[0] = var;
	}
	memoryBarrierShared();
    var = reduction[0];
}

// Random generator from here http://asgerhoedt.dk/?p=323
float random(uint a) {
	a = (a + 0x7ed55d16) + (a << 12);
	a = (a ^ 0xc761c23c) ^ (a >> 19);
	a = (a + 0x165667b1) + (a << 5);
	a = (a + 0xd3a2646c) ^ (a << 9);
	a = (a + 0xfd7046c5) + (a << 3);
	a = (a ^ 0xb55a4f09) ^ (a >> 16);

	return float(a) / float(UINT_MAX);
}

float add_random(
	const float value,
	const int id,
	const int sub_vector,
	const int feature_buffer,
	const int frame_number) {
	return value + NOISE_AMOUNT * 2.f * (random(id + sub_vector * LOCAL_SIZE +
		feature_buffer * BLOCK_EDGE_LENGTH * BLOCK_EDGE_LENGTH +
		frame_number * BUFFER_COUNT * BLOCK_EDGE_LENGTH * BLOCK_EDGE_LENGTH) - 0.5f);
}

vec3 RGB_to_YCoCg(vec3 rgb) {
	return vec3 (
		dot(rgb, vec3( 1.f, 2.f, 1.f )),
			dot(rgb, vec3(2.f, 0.f, -2.f )),
			dot(rgb, vec3(-1.f, 2.f, -1.f ))
	);
}

vec3 YCoCg_to_RGB(vec3 YCoCg) {
	return vec3(
		dot(YCoCg, vec3(0.25f, 0.25f, -0.25f )),
			dot(YCoCg, vec3(0.25f, 0.f, 0.25f)),
			dot(YCoCg, vec3(0.25f, -0.25f, -0.25f))
	);
}

float scale(float value, float min, float max) {
	if (abs(max - min) > 1.0f) {
		return (value - min) / (max - min);
	}
	return value - min;
}
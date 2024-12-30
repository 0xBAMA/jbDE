#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

// tally buffers
layout( binding = 0, r32ui ) uniform uimage2D redTally;
layout( binding = 1, r32ui ) uniform uimage2D greenTally;
layout( binding = 2, r32ui ) uniform uimage2D blueTally;
layout( binding = 3, r32ui ) uniform uimage2D sampleTally;

// depth buffer
layout( binding = 4, r32ui ) uniform uimage2D depthBuffer;

// transparent line buffer
struct line_t {
	vec4 p0;
	vec4 p1;
	vec4 color;
};

layout( binding = 1, std430 ) buffer transparentLineData {
	line_t transparentData[];
};

void main() {
	uint index = gl_GlobalInvocationID.x + 4096 * gl_GlobalInvocationID.y;

}
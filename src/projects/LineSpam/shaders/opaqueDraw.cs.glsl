#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

// depth buffer

// id buffer

// opaque line buffer
struct line_t {
	vec4 p0;
	vec4 p1;
	vec4 color;
};

layout( binding = 0, std430 ) buffer opaqueLineData {
	line_t opaqueData[];
};

void main() {
	uint index = gl_GlobalInvocationID.x + 4096 * gl_GlobalInvocationID.y;

	// writing id values for pixels which compute the same depth value stored in the depth buffer after pre-z
		// setting id to invocation id + 1, so we have 0 as a reserve value
}
#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;

// depth buffer

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
	
}
#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// CA state buffers
layout( binding = 2, r32ui ) uniform uimage2D backBuffer;
layout( binding = 3, r32ui ) uniform uimage2D frontBuffer;

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	// read state + neighborhood from back buffer

	// determine new state

	// write the data to the front buffer

}

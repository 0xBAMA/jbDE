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
	uint previousState = imageLoad( backBuffer, writeLoc ).r;

	uint count = 0;
	count += imageLoad( backBuffer, writeLoc + ivec2( -1, -1 ) ).r;
	count += imageLoad( backBuffer, writeLoc + ivec2(  0, -1 ) ).r;
	count += imageLoad( backBuffer, writeLoc + ivec2(  1, -1 ) ).r;
	count += imageLoad( backBuffer, writeLoc + ivec2( -1,  0 ) ).r;
	// skip center pixel, already exists in previousState
	count += imageLoad( backBuffer, writeLoc + ivec2(  1,  0 ) ).r;
	count += imageLoad( backBuffer, writeLoc + ivec2( -1,  1 ) ).r;
	count += imageLoad( backBuffer, writeLoc + ivec2(  0,  1 ) ).r;
	count += imageLoad( backBuffer, writeLoc + ivec2(  1,  1 ) ).r;

	// determine new state
	uint newState = 0;
	if ( ( count == 2 || count == 3 ) && previousState == 1 ) {
		newState = 1;
	} else if ( count == 3 && previousState == 0 ) {
		newState = 1;
	}

	// write the data to the front buffer
	imageStore( frontBuffer, writeLoc, uvec4( newState, 1, 1, 1 ) );
}

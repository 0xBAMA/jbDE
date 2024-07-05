#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

uniform int previousSlice;
layout( rgba8ui ) uniform uimage3D ddatex;

bool getState ( ivec3 location ) {
	// read state from back buffer
	uint previousState = ( ( imageLoad( ddatex, location ).a ) == 255 ) ? 1 : 0;

	// read neighborhood values from back buffer
	uint count = 0;
	count += ( ( imageLoad( ddatex, location + ivec3( -1, -1,  0 ) ).a ) == 255 ) ? 1 : 0;
	count += ( ( imageLoad( ddatex, location + ivec3(  0, -1,  0 ) ).a ) == 255 ) ? 1 : 0;
	count += ( ( imageLoad( ddatex, location + ivec3(  1, -1,  0 ) ).a ) == 255 ) ? 1 : 0;
	count += ( ( imageLoad( ddatex, location + ivec3( -1,  0,  0 ) ).a ) == 255 ) ? 1 : 0;
	// skip center pixel, already exists in previousState
	count += ( ( imageLoad( ddatex, location + ivec3(  1,  0,  0 ) ).a ) == 255 ) ? 1 : 0;
	count += ( ( imageLoad( ddatex, location + ivec3( -1,  1,  0 ) ).a ) == 255 ) ? 1 : 0;
	count += ( ( imageLoad( ddatex, location + ivec3(  0,  1,  0 ) ).a ) == 255 ) ? 1 : 0;
	count += ( ( imageLoad( ddatex, location + ivec3(  1,  1,  0 ) ).a ) == 255 ) ? 1 : 0;

	// determine new state - Conway's Game of Life rules
	return ( ( count == 2 || count == 3 ) && previousState == 1 ) || ( count == 3 && previousState == 0 );
}

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	uint state = ( getState( ivec3( writeLoc, previousSlice ) ) ? 255 : 0 );
	uvec3 color = imageLoad( ddatex, ivec3( writeLoc, previousSlice ) ).rgb;

	// // mitigation of blinkers, making constant value columns
	// if ( state == 255 ) {
	// 	int sum = 0;
	// 	for ( int i = 0; i < 10; i++ ) {
	// 		sum += ( imageLoad( ddatex, ivec3( writeLoc, previousSlice - i ) ).a == 255 ) ? 0 : 1;
	// 	}
	// 	if ( sum <= 2 ) {
	// 		state = 0;
	// 	}
	// }

	// write the data to the front buffer
	imageStore( ddatex, ivec3( writeLoc, previousSlice + 1 ), uvec4( color, state ) );
}
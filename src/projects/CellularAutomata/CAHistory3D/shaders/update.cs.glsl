#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

uniform int sliceOffset;
layout( r8ui ) uniform uimage3D CAStateBuffer;

ivec3 getOffsetPos ( ivec3 pos ) {
	pos.z += sliceOffset;
	pos.z = pos.z % imageSize( CAStateBuffer ).z;
	return pos;
}

uint getValue ( ivec3 pos ) {
	return imageLoad( CAStateBuffer, getOffsetPos( pos ) ).r;
}

bool getState ( ivec3 location ) {
	// read state from back buffer
	uint previousState = ( getValue( location ) != 0 ) ? 1 : 0;

	// read neighborhood values from back buffer
	uint count = 0;
	count += ( getValue( location + ivec3( -1, -1,  0 ) ) != 0 ) ? 1 : 0;
	count += ( getValue( location + ivec3(  0, -1,  0 ) ) != 0 ) ? 1 : 0;
	count += ( getValue( location + ivec3(  1, -1,  0 ) ) != 0 ) ? 1 : 0;
	count += ( getValue( location + ivec3( -1,  0,  0 ) ) != 0 ) ? 1 : 0;
	// skip center pixel, already exists in previousState
	count += ( getValue( location + ivec3(  1,  0,  0 ) ) != 0 ) ? 1 : 0;
	count += ( getValue( location + ivec3( -1,  1,  0 ) ) != 0 ) ? 1 : 0;
	count += ( getValue( location + ivec3(  0,  1,  0 ) ) != 0 ) ? 1 : 0;
	count += ( getValue( location + ivec3(  1,  1,  0 ) ) != 0 ) ? 1 : 0;

	// determine new state - Conway's Game of Life rules
	return ( ( count == 2 || count == 3 ) && previousState == 1 ) || ( count == 3 && previousState == 0 );
}

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	uint state = ( getState( ivec3( writeLoc, -1 ) ) ? 255 : 0 );
	// uint state = getValue( ivec3( writeLoc, 0 ) ) != 0 ? 255 : 0;
	// uint state = getValue( ivec3( writeLoc, -1 ) );

	// write the data to the front buffer
	imageStore( CAStateBuffer, getOffsetPos( ivec3( writeLoc, 0 ) ), uvec4( state ) );
}
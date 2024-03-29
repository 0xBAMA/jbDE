#version 430 core
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;

layout( binding = 1, r32ui ) uniform uimage3D previous;
layout( binding = 2, r32ui ) uniform uimage3D current;

uniform float decayFactor;

void main() {
	// gaussian kernel
	ivec3 pos = ivec3( gl_GlobalInvocationID.xyz );
	uint g = (
		1 * imageLoad( previous, pos + ivec3( -1, -1, -1 ) ).r +
		1 * imageLoad( previous, pos + ivec3(  1, -1, -1 ) ).r +
		1 * imageLoad( previous, pos + ivec3( -1,  1, -1 ) ).r +
		1 * imageLoad( previous, pos + ivec3(  1,  1, -1 ) ).r +
		1 * imageLoad( previous, pos + ivec3( -1, -1,  1 ) ).r +
		1 * imageLoad( previous, pos + ivec3(  1, -1,  1 ) ).r +
		1 * imageLoad( previous, pos + ivec3( -1,  1,  1 ) ).r +
		1 * imageLoad( previous, pos + ivec3(  1,  1,  1 ) ).r +

		2 * imageLoad( previous, pos + ivec3(  1,  0, -1 ) ).r +
		2 * imageLoad( previous, pos + ivec3(  0, -1, -1 ) ).r +
		2 * imageLoad( previous, pos + ivec3(  0,  1, -1 ) ).r +
		2 * imageLoad( previous, pos + ivec3( -1,  0, -1 ) ).r +
		2 * imageLoad( previous, pos + ivec3(  1,  0,  1 ) ).r +
		2 * imageLoad( previous, pos + ivec3(  0, -1,  1 ) ).r +
		2 * imageLoad( previous, pos + ivec3(  0,  1,  1 ) ).r +
		2 * imageLoad( previous, pos + ivec3( -1,  0,  1 ) ).r +
		2 * imageLoad( previous, pos + ivec3(  1,  1,  0 ) ).r +
		2 * imageLoad( previous, pos + ivec3(  1, -1,  0 ) ).r +
		2 * imageLoad( previous, pos + ivec3( -1,  1,  0 ) ).r +
		2 * imageLoad( previous, pos + ivec3( -1, -1,  0 ) ).r +

		4 * imageLoad( previous, pos + ivec3( -1,  0,  0 ) ).r +
		4 * imageLoad( previous, pos + ivec3(  1,  0,  0 ) ).r +
		4 * imageLoad( previous, pos + ivec3(  0, -1,  0 ) ).r +
		4 * imageLoad( previous, pos + ivec3(  0,  1,  0 ) ).r +
		4 * imageLoad( previous, pos + ivec3(  0,  0, -1 ) ).r +
		4 * imageLoad( previous, pos + ivec3(  0,  0,  1 ) ).r +

		8 * imageLoad( previous, pos + ivec3(  0,  0,  0 ) ).r ) / 64;

	imageStore( current, pos, uvec4( uint( decayFactor * g ) ) );
}

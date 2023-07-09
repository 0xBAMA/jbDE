#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform float time;

void main () {
	// basic XOR pattern
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	uint x = uint( writeLoc.x ) % 256;
	uint y = uint( writeLoc.y ) % 256;
	uint xor = ( x ^ y );

	// blue noise, for shits
	vec3 result = ( xor < 128 ) ? imageLoad( blueNoiseTexture, writeLoc % imageSize( blueNoiseTexture ) ).xyz + vec3( sin( time ) / 2.0f + vec3( 0.5f ) ) * 127 : vec3( xor );

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( result / 255.0f, 1.0f ) );
}

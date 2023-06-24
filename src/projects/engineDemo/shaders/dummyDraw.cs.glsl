#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

void main () {
	// basic XOR pattern
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	uint x = uint( writeLoc.x ) % 256;
	uint y = uint( writeLoc.y ) % 256;
	vec3 result = uvec3( x ^ y ) / 2;

	// add some blue noise, for shits
	result *= imageLoad( blueNoiseTexture, writeLoc % imageSize( blueNoiseTexture ) ).xyz / 255.0f;

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( result / 255.0f, 1.0f ) );
}

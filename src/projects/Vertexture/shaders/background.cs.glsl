#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba8ui ) uniform uimage2D accumulatorTexture;

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	uvec3 result = imageLoad( blueNoiseTexture, writeLoc % imageSize( blueNoiseTexture ) ).xyz;
	result = uvec3( result * 0.1618f );
	// result = uvec3( result * 0.1618f );
	result = uvec3( 0 );
	imageStore( accumulatorTexture, writeLoc, uvec4( result, 255 ) );
}

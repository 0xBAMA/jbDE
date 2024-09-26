#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform usampler2D image;

uniform float time;

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec3 col = vec3( 0.0f );

	col = texelFetch( image, writeLoc, 0 ).rrr;

	// textureQueryLevels gives number of mipmaps... not sure if that's really useful right now

	// so we want to iterate through the mip chain, till we find... what?
	// for ( int i = 0; i <= 10; i++ ) { // 2^10, for 1024x1024
	// 	// ... is this the max level of refinement?
	// 		// I think I actually need twice as many mips as a standard mip chain, to do this the way I want to

	// 	// then it's texelFetch( ... )
	// }

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}

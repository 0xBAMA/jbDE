#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 2, r32ui ) uniform uimage2D dataTexture;

uniform uint maxValue;
uniform float power;

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	// figure out where we are ... sqrt()? tbd
	vec3 col = vec3( 0.0f, pow( float( imageLoad( dataTexture, writeLoc / 4 ).r ) / float( maxValue ), power ), 0.0f );

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}

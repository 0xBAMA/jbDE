#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform float time;

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	// blue noise pattern
	vec4 blue = imageLoad( blueNoiseTexture, writeLoc % imageSize( blueNoiseTexture ) ) / 255.0f;
	blue = vec4( blue.xyz / 5.0f, 1.0f );

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, blue );
}

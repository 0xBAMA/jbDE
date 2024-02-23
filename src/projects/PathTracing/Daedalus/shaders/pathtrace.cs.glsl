#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( location = 0, rgba8ui ) uniform uimage2D blueNoise;
layout( location = 1, rgba32f ) uniform image2D accumulatorColor;
layout( location = 2, rgba32f ) uniform image2D accumulatorNormalsAndDepth;

uniform ivec2 tileOffset;
uniform ivec2 noiseOffset;
uniform int wangSeed;

void main() {
	const ivec2 location = ivec2( gl_GlobalInvocationID.xy ) + tileOffset.xy;
	imageStore( accumulatorColor, location, vec4( 1.0f, vec2( location.xy ) / vec2( imageSize( accumulatorColor ) ), 1.0f ) );
	imageStore( accumulatorNormalsAndDepth, location, vec4( 1.0f, 0.0f, 0.0f, 1.0f ) );
}
#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( location = 0, rgba8ui ) uniform uimage2D blueNoise;
layout( location = 1, rgba32f ) uniform image2D accumulatorColor;
layout( location = 2, rgba32f ) uniform image2D accumulatorNormalsAndDepth;
layout( location = 3, rgba32f ) uniform image2D tonemappedResult;

void main() { // This is where tonemapping etc will be happening on the accumulated image
	const ivec2 location = ivec2( gl_GlobalInvocationID.xy );
	imageStore( tonemappedResult, location, imageLoad( accumulatorColor, location ) );
}
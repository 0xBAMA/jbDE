#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 2, r32ui ) uniform uimage2D ifsAccumulator;

#include "colorRamps.glsl.h"

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	// opportunity here, to do some remapping
	vec3 col = refPalette( imageLoad( ifsAccumulator, writeLoc ).r / 1000.0f, MAGMA ).rgb;

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}

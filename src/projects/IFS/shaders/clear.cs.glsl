#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 2, r32ui ) uniform uimage2D ifsAccumulator;

layout( binding = 0, std430 ) buffer maxBuffer { uint maxCount; };

#include "colorRamps.glsl.h"
#include "mathUtils.h"

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	// write the data to the image
	imageStore( ifsAccumulator, writeLoc, uvec4( 0 ) );
}

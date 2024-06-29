#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform float time;

layout( binding = 0, std430 ) readonly buffer signalData	{ float inputData[]; };
layout( binding = 1, std430 ) readonly buffer fftData		{ float outputData[]; };

#include "mathUtils.h"

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	vec3 col = vec3( 0.0f );

	if ( writeLoc.y < 100 ) {
		float signalValue = inputData[ writeLoc.x ];
		col = vec3( saturate( signalValue ), 0.0f, saturate( -signalValue ) );
	// } else if () {

	}

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}

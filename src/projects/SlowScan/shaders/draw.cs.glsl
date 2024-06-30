#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform float time;

uniform int dataSize;
layout( binding = 0, std430 ) readonly buffer signalData	{ float inputData[]; };
layout( binding = 1, std430 ) readonly buffer fftData		{ float outputData[]; };

#include "mathUtils.h"

void main () {
	// pixel location
	const ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	const ivec2 sampleLoc = writeLoc; // make it bigger

	// pull data from the buffer
	const float signalValue = inputData[ sampleLoc.x ];

	// dc offset is in fftData[ 0 ], also remember that real signals are mirrored, here, so we take the top half
		// note also that the lowest frequencies are towards the top end of the array, so it's inverted
	const int idx = sampleLoc.x / 2;
	const float fftValue = outputData[ idx ];

	vec3 col = vec3( 0.0f );
	if ( sampleLoc.x < dataSize ) {
		if ( sampleLoc.y < 100 ) { // polarized signal display
			col = vec3( saturate( signalValue ), 0.0f, saturate( -signalValue ) );
		} else if ( sampleLoc.y < 200 ) { // waveform graph
			const float graphY = RangeRemapValue( float( sampleLoc.y ), 100.0f, 200.0f, 1.0f, -1.0f );
			if ( abs( graphY - signalValue ) < 0.05f ) {
				col = vec3( 1.0f );
			} else if ( abs( graphY ) < 0.025f ) { // axis marker
				col = vec3( 0.1618f );
			}
		} else if ( sampleLoc.y < 500 ) { // show the fft data
			const float graphY = RangeRemapValue( float( sampleLoc.y ), 200.0f, 500.0f, 200.0f, 0.0f );
			if ( graphY < fftValue ) {
				col = vec3( 1.0f );
			}
		}
	}

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}

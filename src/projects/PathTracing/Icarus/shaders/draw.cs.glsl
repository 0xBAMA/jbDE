#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform float time;

#include "spaceFillingCurves.h.glsl"
#include "colorRamps.glsl.h"

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	// // round up to next power of two
	// ivec2 loc = ihilbert( int( 1000.0f * time + xy2z( writeLoc ) ), int( ceil( log( min( imageSize( accumulatorTexture ).x, imageSize( accumulatorTexture ).y ) ) / log( 2 ) ) ) ); // linear ramps
	// // ivec2 loc = z2xy( int( 1000.0f * time + hilbert( writeLoc, int( ceil( log( min( imageSize( accumulatorTexture ).x, imageSize( accumulatorTexture ).y ) ) / log( 2 ) ) ) ) ) );
	// uint x = uint( loc.x ) % 512;
	// uint y = uint( loc.y ) % 512;
	// uint xor = ( x & y );
	// vec3 col = refPalette( xor / 511.0f, BLUESR ).rgb;

	// // vec3 col = refPalette( abs( sin( time + 0.000618f * hilbert( writeLoc / 5, int( ceil( log( min( imageSize( accumulatorTexture ).x, imageSize( accumulatorTexture ).y ) ) / log( 2 ) ) ) ) ) ), BLUESR ).rgb;

	vec3 col = vec3( 0.0f );

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}

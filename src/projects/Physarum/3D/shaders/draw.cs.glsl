#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// current pheremone buffer
uniform usampler3D continuum;

// viewer state
uniform vec3 viewerPosition;
uniform vec3 viewerBasisX;
uniform vec3 viewerBasisY;
uniform vec3 viewerBasisZ;
uniform float viewerFoV;

#include "consistentPrimitives.glsl.h"

// tbd, controls for this
const int numBounces = 8;

void main () {
	vec2 uv = ( ( gl_GlobalInvocationID.xy + vec2( 0.5f ) ) / vec2( imageSize( accumulatorTexture ).xy ) ) - vec2( 0.5f );

	const float aspectRatio = float( imageSize( accumulatorTexture ).x ) / float( imageSize( accumulatorTexture ).y );
	uv.x *= aspectRatio;

	vec3 color = vec3( 0.0f );

	vec3 rayOrigin = viewerPosition;
	vec3 rayDirection = normalize( uv.x * viewerBasisX + uv.y * viewerBasisY + ( 1.0f / viewerFoV ) * viewerBasisZ );

	vec3 normal = vec3( 0.0f );
	float boxDist = iBoxOffset( rayOrigin, rayDirection, normal, vec3( 1.0f ), vec3( 0.0f ) );

	if ( boxDist > 0.0f && boxDist < MAX_DIST ) { // the ray hits the box
		for ( int bounce = 0; bounce < numBounces; bounce++ ) { // for N bounces
			// start the DDA traversal...

				// each step generate number 0..1

				// if the density at the current voxel is less than the number
					// this ray becomes scattered...
					// attenuate by some amount related to the brightness...
					// ray direction becomes a random unit vector

				// if you leave the volume...
					// if you hit the light...

		}
	}

	// blending, eventually
	imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( color, 1.0f ) );
}

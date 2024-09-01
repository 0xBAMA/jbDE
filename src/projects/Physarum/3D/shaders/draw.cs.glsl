#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 0, r32ui ) uniform uimage3D continuum;

// viewer state
uniform vec3 viewerPosition;
uniform vec3 viewerBasisX;
uniform vec3 viewerBasisY;
uniform vec3 viewerBasisZ;
uniform float viewerFoV;

#include "consistentPrimitives.glsl.h"
#include "mathUtils.h"

bool inBounds ( ivec3 pos ) { // helper function for bounds checking
	return all( greaterThanEqual( pos, ivec3( 0 ) ) ) && all( lessThan( pos, imageSize( continuum ) ) );
}

void main () {
	vec2 uv = ( ( gl_GlobalInvocationID.xy + vec2( 0.5f ) ) / vec2( imageSize( accumulatorTexture ).xy ) ) - vec2( 0.5f );

	const float aspectRatio = float( imageSize( accumulatorTexture ).x ) / float( imageSize( accumulatorTexture ).y );
	uv.x *= aspectRatio;

	vec3 color = vec3( 0.0f );

	vec3 rayOrigin = viewerPosition;
	vec3 rayDirection = normalize( uv.x * viewerBasisX + uv.y * viewerBasisY + ( 1.0f / viewerFoV ) * viewerBasisZ );

	vec3 normal = vec3( 0.0f );
	ivec3 boxSize = imageSize( continuum );
	float boxDist = iBoxOffset( rayOrigin, rayDirection, normal, vec3( boxSize / 2.0f ), vec3( boxSize ) / 2.0f );

	if ( boxDist > 0.0f && boxDist < MAX_DIST ) { // the ray hits the box
		vec3 hitPos = rayOrigin + rayDirection * ( boxDist + 0.01f );

		color += 0.02f;

		// // the checkerboard
		// bool blackOrWhite = ( step( 0.0f,
		// 	cos( pi * hitPos.x + pi / 2.0f ) *
		// 	cos( pi * hitPos.y + pi / 2.0f ) *
		// 	cos( pi * hitPos.z + pi / 2.0f ) ) == 0 );
		// color = vec3( blackOrWhite ? 1.0f : 0.0f );

		// tbd, controls for this
		const int numBounces = 1;
		for ( int bounce = 0; bounce < numBounces; bounce++ ) { // for N bounces
			// start the DDA traversal...
			// from https://www.shadertoy.com/view/7sdSzH
			vec3 deltaDist = 1.0f / abs( rayDirection );
			ivec3 rayStep = ivec3( sign( rayDirection ) );
			bvec3 mask0 = bvec3( false );
			ivec3 mapPos0 = ivec3( floor( hitPos + 0.001f ) );
			vec3 sideDist0 = ( sign( rayDirection ) * ( vec3( mapPos0 ) - hitPos ) + ( sign( rayDirection ) * 0.5f ) + 0.5f ) * deltaDist;

			#define MAX_RAY_STEPS 600
			// for ( int i = 0; i < MAX_RAY_STEPS && inBounds( mapPos0 ); i++ ) {
			for ( int i = 0; i < MAX_RAY_STEPS; i++ ) {

				bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
				vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
				ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

				// each step generate number 0..1

				// if the density at the current voxel is less than the number
					// this ray becomes scattered...
					// attenuate by some amount related to the brightness...
					// ray direction becomes a random unit vector

				// if you leave the volume...
					// if you hit the light...

				uint read = imageLoad( continuum, mapPos0 ).r;
				if ( read != 0 ) {
					// color.b = 1.0f;
					color = mapPos0 / 512.0f;
					break;
				}

				sideDist0 = sideDist1;
				mapPos0 = mapPos1;
			}
		}
	}

	// blending, eventually
	imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( color, 1.0f ) );
}

#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 0, r32ui ) uniform uimage3D continuum;

// rng
uniform int wangSeed;
#include "random.h"

// viewer state
uniform vec3 viewerPosition;
uniform vec3 viewerBasisX;
uniform vec3 viewerBasisY;
uniform vec3 viewerBasisZ;
uniform float viewerFoV;

uniform vec3 skyColor1;
uniform vec3 skyColor2;

uniform bool accumulate;

// scattering density threshold
uniform int densityThreshold;

#include "consistentPrimitives.glsl.h"
#include "mathUtils.h"

bool inBounds ( ivec3 pos ) { // helper function for bounds checking
	return ( all( greaterThanEqual( pos, ivec3( 0 ) ) ) && all( lessThan( pos, imageSize( continuum ) ) ) );
}

void main () {
	seed = wangSeed + 420 * gl_GlobalInvocationID.x + 69 * gl_GlobalInvocationID.y;

	vec2 uv = ( ( gl_GlobalInvocationID.xy + vec2( NormalizedRandomFloat(), NormalizedRandomFloat() ) ) / vec2( imageSize( accumulatorTexture ).xy ) ) - vec2( 0.5f );

	const float aspectRatio = float( imageSize( accumulatorTexture ).x ) / float( imageSize( accumulatorTexture ).y );
	uv.x *= aspectRatio;

	vec3 color = vec3( 0.0f );

	vec3 rayOrigin = viewerPosition;
	vec3 rayDirection = normalize( uv.x * viewerBasisX + uv.y * viewerBasisY + ( 1.0f / viewerFoV ) * viewerBasisZ );

	vec3 normal = vec3( 0.0f );
	ivec3 boxSize = imageSize( continuum );
	float boxDist = iBoxOffset( rayOrigin, rayDirection, normal, vec3( boxSize / 2.0f ), vec3( boxSize ) / 2.0f );

	if ( boxDist < MAX_DIST ) { // the ray hits the box
		// calculating hit position
		vec3 hitPos = rayOrigin + rayDirection * ( boxDist + 0.01f );

		// trying to handle viewer-inside-volume
		if ( inBounds( ivec3( viewerPosition ) ) ) {
			hitPos = rayOrigin;
		}

		vec3 transmission = vec3( 1.0f );

		// tbd, controls for this
		const int numBounces = 3;
		for ( int bounce = 0; bounce < numBounces; bounce++ ) { // for N bounces
			// start the DDA traversal...
			// from https://www.shadertoy.com/view/7sdSzH
			vec3 deltaDist = 1.0f / abs( rayDirection );
			ivec3 rayStep = ivec3( sign( rayDirection ) );
			bvec3 mask0 = bvec3( false );
			ivec3 mapPos0 = ivec3( floor( hitPos ) );
			vec3 sideDist0 = ( sign( rayDirection ) * ( vec3( mapPos0 ) - hitPos ) + ( sign( rayDirection ) * 0.5f ) + 0.5f ) * deltaDist;

			#define MAX_RAY_STEPS 600
			for ( int i = 0; i < MAX_RAY_STEPS && inBounds( mapPos0 ); i++ ) {

				bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
				vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
				ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;


				// each step generate number 0..1, chance to scatter

				// linear falloff
				// uint densityRead = imageLoad( continuum, mapPos0 % ivec3( imageSize( continuum ) ) ).r;
				// if ( NormalizedRandomFloat() < ( densityRead / float( densityThreshold ) ) ) {

				// doing Beer's law
				float densityRead = exp( -float( imageLoad( continuum, mapPos0 ).r ) / float( densityThreshold ) );
				if ( NormalizedRandomFloat() > densityRead ) {

				// if the density at the current voxel is less than the number this ray becomes scattered

			// need to figure out how we are doing materials stuff:
				// ============================================================================
				// transmission

					// constant
					// transmission *= 0.9f;

					// density based
					// transmission *= saturate( 1.0f - densityRead / float( densityThreshold ) );
					transmission *= saturate( densityRead );
				// ============================================================================
				// direction

					// weighted by rayDirection, it scatters mostly forward
					rayDirection = normalize( rayDirection + RandomUnitVector() ); // like a more typical phase function

					// weighted by -rayDirection, scatters mostly backwards
					// rayDirection = normalize( -rayDirection + RandomUnitVector() ); // "diffuse" type behavior

					// with a random unit vector, it's scattering in any direction
					// rayDirection = RandomUnitVector();
				// ============================================================================

					rayStep = ivec3( sign( rayDirection ) );
					mask0 = bvec3( false );
					// mapPos0 = ivec3( floor( hitPos + 0.001f ) ); // doesn't need to be updated
					sideDist0 = ( sign( rayDirection ) * ( vec3( 0.5f ) ) + ( sign( rayDirection ) * 0.5f ) + 0.5f ) * deltaDist;

				} else {
					// take a regular step
					sideDist0 = sideDist1;
					mapPos0 = mapPos1;
				}
			}
		}

		if ( transmission != vec3( 1.0f ) ) {
			if ( dot( rayDirection, vec3( 0.0f, 1.0f, 0.0f ) ) < 0.0f ) {
				color = transmission * skyColor1;
			} else {
				color = transmission * skyColor2;
			}
		}
	}

	vec4 previousColor = imageLoad( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ) );
	previousColor.a += 1.0f;
	if ( accumulate ) {
		imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( mix( previousColor.rgb, color, 1.0f / previousColor.a ), previousColor.a ) );
	} else {
		imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( mix( previousColor.rgb, color, 0.25f ), 1.0f ) );
	}
}

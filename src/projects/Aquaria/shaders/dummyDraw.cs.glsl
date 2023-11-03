#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

layout( binding = 2, rg32ui ) uniform uimage3D idxBuffer;
layout( binding = 3, rgba16f ) uniform image3D dataCacheBuffer;

uniform float time;
uniform vec2 resolution;
uniform float scale;
uniform mat3 invBasis;
uniform vec3 blockSize;

#define PI 3.1415f

#include "intersect.h"

// ==============================================================================================
vec3 eliNormal ( in vec3 pos, in vec3 center, in vec3 radii ) {
	return normalize( ( pos - center ) / ( radii * radii ) );
}
float eliIntersect ( in vec3 ro, in vec3 rd, in vec3 center, in vec3 radii ) {
	vec3 oc = ro - center;
	vec3 ocn = oc / radii;
	vec3 rdn = rd / radii;
	float a = dot( rdn, rdn );
	float b = dot( ocn, rdn );
	float c = dot( ocn, ocn );
	float h = b * b - a * ( c - 1.0f );
	if ( h < 0.0f ) return -1.0f;
	return ( -b - sqrt( h ) ) / a;
}

float RemapRange ( const float value, const float iMin, const float iMax, const float oMin, const float oMax ) {
	return ( oMin + ( ( oMax - oMin ) / ( iMax - iMin ) ) * ( value - iMin ) );
}

uniform ivec2 noiseOffset;
float blueNoiseRead ( ivec2 loc, int idx ) {
	ivec2 wrappedLoc = ( loc + noiseOffset ) % imageSize( blueNoiseTexture );
	uvec4 sampleValue = imageLoad( blueNoiseTexture, wrappedLoc );
	switch ( idx ) {
	case 0:
		return sampleValue.r / 255.0f;
		break;
	case 1:
		return sampleValue.g / 255.0f;
		break;
	case 2:
		return sampleValue.b / 255.0f;
		break;
	case 3:
		return sampleValue.a / 255.0f;
		break;
	}
}


void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec3 prevColor = imageLoad( accumulatorTexture, writeLoc ).rgb;
	vec3 col = vec3( 0.0f );

	// remapped uv
	vec2 subpixelOffset = vec2(
		blueNoiseRead( ivec2( gl_GlobalInvocationID.xy ), 1 ),
		blueNoiseRead( ivec2( gl_GlobalInvocationID.xy ), 2 )
	);
	vec2 uv = ( vec2( gl_GlobalInvocationID.xy ) + subpixelOffset ) / resolution.xy;
	uv = ( uv - 0.5f ) * 2.0f;

	// aspect ratio correction
	uv.x *= ( resolution.x / resolution.y );

	// box intersection
	float tMin, tMax;
	vec3 Origin = invBasis * vec3( scale * uv, -2.0f );
	vec3 Direction = invBasis * normalize( vec3( uv * 0.1f, 2.0f ) );

// if refraction is desired:
// #define REFRACTIVE_BUBBLE 1

#ifdef REFRACTIVE_BUBBLE
	float sHit = eliIntersect( Origin, Direction, vec3( 0.0f ), ( blockSize / 2.0f ) );
	if ( sHit > 0.0f ) {
		// update ray position to be at the sphere's surface
		Origin = Origin + sHit * Direction;
		// update ray direction to the refracted ray
		Direction = refract( Direction, eliNormal( Origin, vec3( 0.0f ), blockSize / 2.0f ), 4.0f );
#endif

		// then intersect with the AABB
		const bool hit = Intersect( Origin, Direction, -blockSize / 2.0f, blockSize / 2.0f, tMin, tMax );

		// what are the dimensions
		const ivec3 blockDimensions = imageSize( dataCacheBuffer );

		const float fogScalar = 0.0002f;

		if ( hit ) { // texture sample
			// for trimming edges
			const float epsilon = 0.001f;
			const vec3 hitpointMin = Origin + tMin * Direction;
			const vec3 hitpointMax = Origin + tMax * Direction;
			const vec3 blockUVMin = vec3(
				RemapRange( hitpointMin.x, -blockSize.x / 2.0f, blockSize.x / 2.0f, 0 + epsilon, blockDimensions.x - epsilon ),
				RemapRange( hitpointMin.y, -blockSize.y / 2.0f, blockSize.y / 2.0f, 0 + epsilon, blockDimensions.y - epsilon ),
				RemapRange( hitpointMin.z, -blockSize.z / 2.0f, blockSize.z / 2.0f, 0 + epsilon, blockDimensions.z - epsilon )
			);

			const vec3 blockUVMax = vec3(
				RemapRange( hitpointMax.x, -blockSize.x / 2.0f, blockSize.x / 2.0f, 0 + epsilon, blockDimensions.x - epsilon ),
				RemapRange( hitpointMax.y, -blockSize.y / 2.0f, blockSize.y / 2.0f, 0 + epsilon, blockDimensions.y - epsilon ),
				RemapRange( hitpointMax.z, -blockSize.z / 2.0f, blockSize.z / 2.0f, 0 + epsilon, blockDimensions.z - epsilon )
			);

			// confirm good hit - highlight volume, with thickness
			const float thickness = abs( tMin - tMax );
			// col = vec3( 1.0f - exp( -thickness *  5.0f ) );

			// DDA traversal
			// from https://www.shadertoy.com/view/7sdSzH
			vec3 deltaDist = 1.0f / abs( Direction );
			ivec3 rayStep = ivec3( sign( Direction ) );
			bvec3 mask0 = bvec3( false );
			ivec3 mapPos0 = ivec3( floor( blockUVMin + 0.0f ) );
			vec3 sideDist0 = ( sign( Direction ) * ( vec3( mapPos0 ) - blockUVMin ) + ( sign( Direction ) * 0.5f ) + 0.5f ) * deltaDist;

			#define MAX_RAY_STEPS 1200
			for ( int i = 0; i < MAX_RAY_STEPS && ( all( greaterThanEqual( mapPos0, ivec3( 0 ) ) ) && all( lessThan( mapPos0, imageSize( dataCacheBuffer ) ) ) ); i++ ) {
				// Core of https://www.shadertoy.com/view/4dX3zl Branchless Voxel Raycasting
				bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
				vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
				ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

				vec4 read = imageLoad( dataCacheBuffer, mapPos0 );
				if ( read.a != 0.0f ) { // this should be the hit condition
					col = vec3( 1.0f - exp( -i * fogScalar ) ) + read.rgb;
					// col = read.rgb;
					break;
				} else {
					col = vec3( 1.0f - exp( -i * fogScalar ) );
				}

				sideDist0 = sideDist1;
				mapPos0 = mapPos1;
			}
		}
#ifdef REFRACTIVE_BUBBLE
	}
#endif

	// write the data to the image
	// imageStore( accumulatorTexture, writeLoc, vec4( mix( col, prevColor, 0.9f ), 1.0f ) );
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}

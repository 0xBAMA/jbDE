#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;
//=============================================================================================================================
#include "random.h"
#include "noise.h"
#include "hg_sdf.glsl"
#include "pbrConstants.glsl"
#include "rayState2.h.glsl"
//=============================================================================================================================
layout( binding = 1, std430 ) readonly buffer rayState { rayState_t state[]; };
layout( binding = 2, std430 ) writeonly buffer intersectionBuffer { intersection_t intersectionScratch[]; };
//=============================================================================================================================
mat2 rot2(in float a){ float c = cos(a), s = sin(a); return mat2(c, s, -s, c); }
float deGyroid ( vec3 p ) {
	float d = 1e5;
	const int n = 3;
	const float fn = float(n);
	for(int i = 0; i < n; i++){
		vec3 q = p;
		float a = float(i)*fn*2.422; //*6.283/fn
		a *= a;
		q.z += float(i)*float(i)*1.67; //*3./fn
		q.xy *= rot2(a);
		float b = (length(length(sin(q.xy) + cos(q.yz))) - .15);
		float f = max(0., 1. - abs(b - d));
		d = min(d, b) - .25*f*f;
	}
	return d;
}
//=============================================================================================================================
const float scaleFactor = 100.0f;
//=============================================================================================================================
vec3 scatterColor;
float GetVolumeDensity( ivec3 pos ) {

	vec3 p = pos / scaleFactor;

	const float scale = 15.0f;
	bool blackOrWhite = ( step( -0.8f,
		// cos( scale * pi * p.x + pi / 2.0f ) *
		// cos( scale * pi * p.y + pi / 2.0f ) *
		// cos( scale * pi * p.z + pi / 2.0f ) ) == 0 );
		cos( scale * pi * p.y + pi / 2.0f ) ) == 0 );

	const float scalar = 0.8f;
	// float d = max( deWhorl( p / 1.0f ), fPlane( p, vec3( 1.0f, 0.0f, 0.0f ), 0.0f ) );
	// const float boxDist = fBox( p, vec3( 10.0f, 3.0f, 12.0f ) );
	// const float boxDist = fBox( p, vec3( 15.0f ) );
	const float boxDist = fBox( p, vec3( 4.0f ) );
	// float d = max( deGyroid( p * scalar ) / scalar, boxDist ) - ( blackOrWhite ? -0.02f : 0.05f );
	float value;

	// if ( d < 0.0f ) {
	// 	value = 200000.0f * -d;
	// 	// scatterColor = vec3( 0.99f );
	// 	scatterColor = bone.brg;
	// } else if ( boxDist < 0.0f ) {
	if ( boxDist < 0.0f ) {
		// value = 1.0f;
		value = 2.618f;
		// value = 5.0f;
		// value = 0.0f;
		// value = 10.0f;
		// scatterColor = mix( tungsten, sapphire, 0.618f );
		scatterColor = vec3( 0.618f );
	} else {
		value = 0.0f;
	}

	return value;
}
//=============================================================================================================================
void main () {
	const rayState_t myState = state[ gl_GlobalInvocationID.x ];

	const uvec2 loc = uvec2( GetPixelIndex( myState ) );
	seed = loc.x * 10625 + loc.y * 23624 + gl_GlobalInvocationID.x * 2335;

	const vec3 origin = GetRayOrigin( myState );
	const vec3 direction = GetRayDirection( myState );

	if ( IsLiving( myState ) ) {

		// DDA traversal to a potential scattering event
		bool hit = false;
		float hitDistance = MAX_DIST;
		vec3 transmission = vec3( 0.0f );
		const vec3 startPos = origin * scaleFactor;

		// start the DDA traversal...
		// from https://www.shadertoy.com/view/7sdSzH
		vec3 deltaDist = 1.0f / abs( direction );
		ivec3 rayStep = ivec3( sign( direction ) );
		bvec3 mask0 = bvec3( false );
		ivec3 mapPos0 = ivec3( floor( startPos ) );
		vec3 sideDist0 = ( sign( direction ) * ( vec3( mapPos0 ) - startPos ) + ( sign( direction ) * 0.5f ) + 0.5f ) * deltaDist;

		#define MAX_RAY_STEPS 3000
		for ( int i = 0; i < MAX_RAY_STEPS && !hit; i++ ) {

			bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
			vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
			ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

			// doing Beer's law - if we pass a threshold check, we scatter
			// int densityRead = max( 0, int( imageLoad( continuum, mapPos0 ).r ) - noiseFloor );
			float densityRead = GetVolumeDensity( mapPos0 ); // constant density for now
			int densityThreshold = 5000;
			float density = exp( -densityRead / float( densityThreshold ) );

			if ( NormalizedRandomFloat() > density ) {

			// materials stuff:
				// // ============================================================================
				// // transmission attenuates by the density
				// 	transmission *= saturate( density );
				transmission = saturate( density ) * scatterColor;
				// transmission = 0.99f;
				// // ============================================================================
				// // direction - positive rdWeight means forward scatter, 0 is random scatter, negative is backscatter
				// 	rayDirection = normalize( viewerScatterWeight * rayDirection + RandomUnitVector() );
				// // ============================================================================

				hit = true;
				hitDistance = length( ivec3( floor( startPos ) ) - mapPos0 ) / scaleFactor;

			} else {
				// take a regular step
				sideDist0 = sideDist1;
				mapPos0 = mapPos1;
			}
		}

		// fill out intersection struct
		intersection_t VolumeIntersection;
		IntersectionReset( VolumeIntersection );

		SetHitAlbedo( VolumeIntersection, transmission );
		SetHitRoughness( VolumeIntersection, 0.0f );
		SetHitDistance( VolumeIntersection, hitDistance );

		// SetHitMaterial( VolumeIntersection, VOLUME ); // this doesn't really mean anything (should it?)
		// SetHitNormal( VolumeIntersection, ... ); // normal doesn't really mean anything here
		SetHitIntersector( VolumeIntersection, VOLUMEHIT ); // this is the key delineation... material seems better, tbd

		if ( hit ) { // this ray hits in the specified range, write back to the buffer
			const int index = int( gl_GlobalInvocationID.x ) * NUM_INTERSECTORS + INTERSECT_VOLUME;
			intersectionScratch[ index ] = VolumeIntersection;
		}
	}
}
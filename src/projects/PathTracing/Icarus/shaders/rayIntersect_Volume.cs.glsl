#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;

// ray state buffer
#include "rayState.h.glsl"
layout( binding = 1, std430 ) buffer rayState { rayState_t state[]; };

#include "random.h"
#include "noise.h"
#include "hg_sdf.glsl"

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

float deWhorl( vec3 p ) {
	float i, e, s, g, k = 0.01;
	p.xy *= mat2( cos( p.z ), sin( p.z ), -sin( p.z ), cos( p.z ) );
	e = 0.3 - dot( p.xy, p.xy );
	for( s = 2.0; s < 2e2; s /= 0.6 ) {
		p.yz *= mat2( cos( s ), sin( s ), -sin( s ), cos( s ) );
		e += abs( dot( sin( p * s * s * 0.2 ) / s, vec3( 1.0 ) ) );
	}
	return e;
}

const vec3 carrot = vec3( 0.713f, 0.170f, 0.026f );
const vec3 honey = vec3( 0.831f, 0.397f, 0.038f );
const vec3 bone = vec3( 0.887f, 0.789f, 0.434f );
const vec3 tire = vec3( 0.023f, 0.023f, 0.023f );
const vec3 sapphire = vec3( 0.670f, 0.764f, 0.855f );
const vec3 nickel = vec3( 0.649f, 0.610f, 0.541f );

vec3 scatterColor;
float GetVolumeDensity( ivec3 pos ) {
	// return 100.0f * perlinfbm( pos / 100.0f, 10.0f, 4 );

	// vec3 p = pos / 300.0f;
	// return ( step( 0.0f,
	// 	cos( pi * p.x + pi / 2.0f ) *
	// 	cos( pi * p.y + pi / 2.0f ) *
	// 	cos( pi * p.z + pi / 2.0f ) ) == 0 ) ? 0.0f : 30.0f;

	// return ( length( vec3( pos ) ) < 300.0f ) ? 350.0f : 0.0f;

	vec3 p = pos / 100.0f;

	// float pModInterval1(inout float p, float size, float start, float stop) {

	float d = deWhorl( p / 1.0f );
	float value;

	if ( d < 0.0f ) {
		value = 200000.0f * -d;
		// value = 10000.0f * -d;
		// scatterColor = vec3( 0.99f );
		scatterColor = vec3( 1.0f );
	} else {
		// value = pi + 0.618f;
		// value = 0.0f;
		value = 100.0f;
		scatterColor = sapphire;
	}

	return value;
}

void main () {
	rayState_t myState = state[ gl_GlobalInvocationID.x ];

	const uvec2 loc = uvec2( GetPixelIndex( myState ) );
	seed = loc.x * 10625 + loc.y * 23624 + gl_GlobalInvocationID.x * 2335;

	const vec3 origin = GetRayOrigin( myState );
	const vec3 direction = GetRayDirection( myState );

	const float scaleFactor = 100.0f;

	// ray is live, do the raymarch...
	if ( length( direction ) > 0.5f ) {
		// DDA traversal to a potential scattering event
		bool hit = false;
		float hitDistance = 300.0f;
		vec3 transmission = vec3( 0.0f );
		const vec3 startPos = origin * scaleFactor;

		// start the DDA traversal...
		// from https://www.shadertoy.com/view/7sdSzH
		vec3 deltaDist = 1.0f / abs( direction );
		ivec3 rayStep = ivec3( sign( direction ) );
		bvec3 mask0 = bvec3( false );
		ivec3 mapPos0 = ivec3( floor( startPos ) );
		vec3 sideDist0 = ( sign( direction ) * ( vec3( mapPos0 ) - startPos ) + ( sign( direction ) * 0.5f ) + 0.5f ) * deltaDist;

		#define MAX_RAY_STEPS 2000
		for ( int i = 0; i < MAX_RAY_STEPS && !hit; i++ ) {

			bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
			vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
			ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

			// doing Beer's law - if we pass a threshold check, we scatter
			// int densityRead = max( 0, int( imageLoad( continuum, mapPos0 ).r ) - noiseFloor );
			float densityRead = GetVolumeDensity( mapPos0 ); // constant density for now
			int densityThreshold = 50000;
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

		if ( hit && hitDistance < GetHitDistance( myState ) ) {
			SetHitAlbedo( myState, transmission );
			SetHitRoughness( myState, 0.0f );
			SetHitDistance( myState, hitDistance );

			// SetHitMaterial( myState, VOLUME ); // this doesn't really mean anything
			// SetHitNormal( myState, ... ); // normal doesn't really mean anything here
			SetHitIntersector( myState, VOLUMEHIT ); // this is the key delineation

			state[ gl_GlobalInvocationID.x ] = myState;
		}
	}
}
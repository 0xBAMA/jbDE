#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;

#include "random.h"

// ray state buffers
#include "rayState.h.glsl"
layout( binding = 1, std430 ) buffer rayState { rayState_t state[]; };

// accumulation buffers
layout( binding = 0, r32ui ) uniform uimage2D rTally;
layout( binding = 1, r32ui ) uniform uimage2D gTally;
layout( binding = 2, r32ui ) uniform uimage2D bTally;
layout( binding = 3, r32ui ) uniform uimage2D count;

vec3 anglePhong( float a, vec3 n ){
	float r1 = NormalizedRandomFloat();
	float r2 = NormalizedRandomFloat();
	float t = pow( r2, 2.0f / ( 1.0f + a ) );
	float x = cos( 2.0f * 3.14159f * r1 ) * sqrt( 1.0f - t );
	float y = sin( 2.0f * 3.14159f * r1 ) * sqrt( 1.0f - t );
	float z = sqrt( t );
	vec3 W = ( abs( n.x ) > 0.99f ) ? vec3( 0.0f, 1.0f, 0.0f ) : vec3( 1.0f, 0.0f, 0.0f );
	vec3 N = n;
	vec3 T = normalize( cross( N, W ) );
	vec3 B = cross( T, N );
	return normalize( x * T + y * B + z * N );
}

void main () {
	rayState_t myState = state[ gl_GlobalInvocationID.x ];

	// location of the associated pixel
	const ivec2 loc = GetPixelIndex( myState );
	bool terminate = false;
	const float epsilon = 0.001f;

	seed = uint( loc.x ) * 10625 + uint( loc.y ) * 23624 + gl_GlobalInvocationID.x * 42069;

	intersection_t closestIntersection;
	IntersectionReset( closestIntersection );

	// We need to do a gather over the N intersection cantidates to find the closest, similar to the Daedalus impl
	for ( int i = 0; i < NUM_INTERSECTORS; i++ ) {
		const int baseIndex = NUM_INTERSECTORS * int( gl_GlobalInvocationID.x );
		intersection_t temp = intersectionScratch[ baseIndex + i ];
		if ( GetHitDistance( temp ) < GetHitDistance( closestIntersection ) ) {
			closestIntersection = temp; // update with nearest intersection
		}
	}

	if ( GetHitIntersector( closestIntersection ) == NOHIT ) {

		// if you didn't any object in the scene, we're saying you hit the skybox
			// this is probably going to be setup very similar to the rectilinear map from Daedalus
		terminate = true;
		const float mixFactor = dot( normalize( vec3( 0.0f, 2.0f, 1.0f ) ), GetRayDirection( myState ) );
		const vec3 color = vec3( smoothstep( 0.18f, 0.9f, mixFactor ) * 5.0f );
		// const vec3 color = vec3( 0.0f );

		AddEnergy( myState, GetTransmission( myState ) * color );

	} else if ( GetHitIntersector( closestIntersection ) == VOLUMEHIT ) {

		// // from Nikolay
		// vec3 other = vec3( NormalizedRandomFloat(), NormalizedRandomFloat(), NormalizedRandomFloat() ) * 2.0f - 1.0f;
		// SetRayDirection( myState, normalize( other + anglePhong( 1800.0f, GetRayDirection( myState ) ) * 0.15f ) );

		SetRayOrigin( myState, GetRayOrigin( myState ) + GetHitDistance( closestIntersection ) * GetRayDirection( myState ) );
		// SetRayDirection( myState, normalize( 0.25f * GetRayDirection( myState ) + RandomUnitVector() ) );
		SetRayDirection( myState, normalize( 2.0f * GetRayDirection( myState ) + RandomUnitVector() ) );
		// SetRayDirection( myState, normalize( GetRayDirection( myState ) * RandomUnitVector() ) );
		// SetRayDirection( myState, normalize( anglePhong( 100.0f, GetRayDirection( myState ) ) ) );
		SetTransmission( myState, GetTransmission( myState ) * GetHitAlbedo( closestIntersection ) );

	} else {
	// material handling...

		// we need to generate a new ray, and continue
		const vec3 origin = GetRayOrigin( myState );
		const vec3 direction = GetRayDirection( myState );
		const float dist = GetHitDistance( closestIntersection );
		const vec3 normal = GetHitNormal( closestIntersection );
		const vec3 albedo = GetHitAlbedo( closestIntersection );

		// material identifier
		const int materialID = GetHitMaterial( closestIntersection );

		// update origin point + epsilon bump (need to exclude epsilon bump for volumetrics, refractive hits)
		SetRayOrigin( myState, origin + dist * direction + 2.0f * epsilon * normal );

		// precomputing the new vectors
		const vec3 diffuseVector = cosWeightedRandomHemisphereDirection( normal );
		const vec3 reflectedVector = reflect( direction, normal );

		switch ( materialID ) {
		case NONE: {}// tbd, this might go away
		break;
		
		case DIFFUSE: { // lambertian diffuse reflection
			SetRayDirection( myState, diffuseVector );
			SetTransmission( myState, GetTransmission( myState ) * albedo );
		}
		break;

		case EMISSIVE: { // light emitting surface
			SetRayDirection( myState, diffuseVector );
			AddEnergy( myState, GetTransmission( myState ) * albedo );
		}
		break;

		case MIRROR: { // mirror reflection
			SetRayDirection( myState, reflectedVector );
			SetTransmission( myState, GetTransmission( myState ) * albedo );
		}
		break;

		default: {}
		break;
		}
	}

	// russian roulette termination
	vec3 transmission = GetTransmission( myState );
	const float maxChannel = max( max( transmission.x, transmission.y ), transmission.z );
	if ( NormalizedRandomFloat() > maxChannel ) {
		terminate = true;
	}
	// RR compensation term
	transmission *= ( 1.0f / maxChannel );
	SetTransmission( myState, transmission );

	if ( terminate ) {

		// time to write to the framebuffer
		const vec3 color = GetEnergyTotal( myState );
		imageAtomicAdd( rTally, loc, uint( 1024 * color.x ) );
		imageAtomicAdd( gTally, loc, uint( 1024 * color.y ) );
		imageAtomicAdd( bTally, loc, uint( 1024 * color.z ) );
		imageAtomicAdd( count, loc, 1 );

		// and the ray is dead
		StateReset( state[ gl_GlobalInvocationID.x ] );

	} else {

		// store updated stuff back in the buffer
		state[ gl_GlobalInvocationID.x ] = myState;

	}
}
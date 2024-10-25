#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;

#include "rayState.h.glsl"
#include "random.h"

// pixel offset + ray state buffers
layout( binding = 1, std430 ) buffer rayState { rayState_t state[]; };

// accumulation buffers
layout( binding = 0, r32ui ) uniform uimage2D rTally;
layout( binding = 1, r32ui ) uniform uimage2D gTally;
layout( binding = 2, r32ui ) uniform uimage2D bTally;
layout( binding = 3, r32ui ) uniform uimage2D count;

void main () {
	rayState_t myState = state[ gl_GlobalInvocationID.x ];
	seed = uint( GetPixelIndex( myState ).x ) * 10625 + uint( GetPixelIndex( myState ).y ) * 23624 + gl_GlobalInvocationID.x * 2335;

	// location of the associated pixel
	const ivec2 loc = GetPixelIndex( myState );
	bool terminate = false;

	const float epsilon = 0.001f;

	if ( GetHitIntersector( myState ) == SDFHIT ) {

		// we need to generate a new ray, and continue
		const vec3 origin = GetRayOrigin( myState );
		const vec3 direction = GetRayDirection( myState );
		const float dist = GetHitDistance( myState );
		const vec3 normal = GetHitNormal( myState );
		const vec3 transmission = GetTransmission( myState );

		// wipe existing values
		StateReset( state[ gl_GlobalInvocationID.x ] );

		// store back, updated
		SetTransmission( state[ gl_GlobalInvocationID.x ], transmission * 0.618f );
		SetRayDirection( state[ gl_GlobalInvocationID.x ], cosWeightedRandomHemisphereDirection( normal ) );
		SetRayOrigin( state[ gl_GlobalInvocationID.x ], origin + dist * direction + 2.0f * epsilon * normal );
		SetPixelIndex( state[ gl_GlobalInvocationID.x ], loc );

	} else {
		// if you didn't hit the object, you die
	// russian roulette termination
	vec3 transmission = GetTransmission( myState );
	const float maxChannel = max( max( transmission.x, transmission.y ), transmission.z );
	if ( NormalizedRandomFloat() > maxChannel ) {
		terminate = true;
	}
	// RR compensation term
	transmission *= ( 1.0f / maxChannel );
	SetTransmission( myState, transmission );

		terminate = true;
	}

	if ( terminate ) {
		const vec3 transmission = GetTransmission( myState );
		const vec3 color = transmission * vec3( 1.0f ); // sky light is 1.0
		
		imageAtomicAdd( rTally, loc, uint( 1024 * color.x ) );
		imageAtomicAdd( gTally, loc, uint( 1024 * color.y ) );
		imageAtomicAdd( bTally, loc, uint( 1024 * color.z ) );
		imageAtomicAdd( count, loc, 1 );

		// and the ray is dead
		StateReset( state[ gl_GlobalInvocationID.x ] );
	}
}
#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;

#include "rayState.h.glsl"

// pixel offset + ray state buffers
layout( binding = 0, std430 ) readonly buffer pixelOffsets { uvec2 offsets[]; };
layout( binding = 1, std430 ) buffer rayState { rayState_t state[]; };

// accumulation buffers
layout( binding = 0, r32ui ) uniform uimage2D rTally;
layout( binding = 1, r32ui ) uniform uimage2D gTally;
layout( binding = 2, r32ui ) uniform uimage2D bTally;
layout( binding = 3, r32ui ) uniform uimage2D count;

void main () {
	rayState_t myState = state[ gl_GlobalInvocationID.x ];

	const float d = GetHitDistance( myState );
	const int didHit = GetHitIntersector( myState );
	const ivec2 loc = GetPixelIndex( myState );
	const vec3 normal = GetHitNormal( myState );

	if ( didHit == 1 ) {
		imageAtomicAdd( rTally, loc, uint( 1024 * abs( normal.x ) * ( 1.0f / d ) ) );
		imageAtomicAdd( gTally, loc, uint( 1024 * abs( normal.y ) * ( 1.0f / d ) ) );
		imageAtomicAdd( bTally, loc, uint( 1024 * abs( normal.z ) * ( 1.0f / d ) ) );
	}
	imageAtomicAdd( count, loc, 1 );
}
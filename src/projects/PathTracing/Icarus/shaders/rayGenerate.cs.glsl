#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;

struct rayState_t {
	// there's some stuff for padding... this is very wip
	vec4 origin;
	vec4 direction;
	vec4 pixel;
};

// pixel offset + ray state buffers
layout( binding = 0, std430 ) readonly buffer pixelOffsets { uvec2 offsets[]; };
layout( binding = 1, std430 ) buffer rayState { rayState_t state[]; };

// accumulation buffers
layout( binding = 0, r32ui ) uniform uimage2D rTally;
layout( binding = 1, r32ui ) uniform uimage2D gTally;
layout( binding = 2, r32ui ) uniform uimage2D bTally;
layout( binding = 3, r32ui ) uniform uimage2D count;

#include "random.h"

void main () {
	ivec2 offset = ivec2( offsets[ gl_GlobalInvocationID.x ] );

	seed = offset.x * 100625 + offset.y * 2324 + gl_GlobalInvocationID.x * 235;

	// testing, making sure we have good offsets
	imageAtomicAdd( rTally, offset, uint( 1024 * NormalizedRandomFloat() ) );
	imageAtomicAdd( gTally, offset, uint( 1024 * NormalizedRandomFloat() ) );
	imageAtomicAdd( bTally, offset, uint( 1024 * NormalizedRandomFloat() ) );
	imageAtomicAdd( count, offset, 1 );
}
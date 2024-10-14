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

void main () {

	rayState_t myState = state[ gl_GlobalInvocationID.x ];

	ivec2 loc = ivec2( myState.pixel.xy );
	if ( myState.origin.w == 1.0f ) {
		imageAtomicAdd( rTally, loc, uint( 1024 * ( 1.0f / myState.direction.w ) ) );
		imageAtomicAdd( gTally, loc, uint( 1024 * ( 1.0f / myState.direction.w ) ) );
		imageAtomicAdd( bTally, loc, uint( 1024 * ( 1.0f / myState.direction.w ) ) );
	}
	imageAtomicAdd( count, loc, 1 );
}
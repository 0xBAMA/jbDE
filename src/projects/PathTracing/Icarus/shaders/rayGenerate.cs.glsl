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

#include "random.h"

uniform vec3 basisX;
uniform vec3 basisY;
uniform vec3 basisZ;
uniform vec3 viewerPosition;

void main () {
	const int index = int( gl_GlobalInvocationID.x );
	ivec2 offset = ivec2( offsets[ index ] );
	seed = offset.x * 100625 + offset.y * 2324 + gl_GlobalInvocationID.x * 235;

	// generate initial ray origins + directions... very basic camera logic
	vec2 uv = ( vec2( offset ) + vec2( NormalizedRandomFloat(), NormalizedRandomFloat() ) ) / vec2( 1920, 1080 );
	uv = uv * 2.0f - vec2( 1.0f );

	const float aspectRatio = 1920.0f / 1080.0f;
	const float FoV = 0.618f;

	// compute ray origin + direction
	state[ index ].origin.xyz = viewerPosition;
	state[ index ].direction = vec4( normalize( aspectRatio * uv.x * basisX + uv.y * basisY + ( 1.0f / FoV ) * basisZ ), 0.0f );

	// need to know what pixel to write to
	state[ index ].pixel.xy = vec2( offset );
}
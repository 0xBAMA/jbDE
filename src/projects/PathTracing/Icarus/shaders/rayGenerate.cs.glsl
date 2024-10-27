#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;

#include "random.h"
#include "rayState.h.glsl"

// pixel offset + ray state buffers
layout( binding = 0, std430 ) readonly buffer pixelOffsets { uvec2 offsets[]; };
layout( binding = 1, std430 ) buffer rayState { rayState_t state[]; };

uniform vec3 basisX;
uniform vec3 basisY;
uniform vec3 basisZ;
uniform vec3 viewerPosition;

uniform float FoV;
uniform vec2 imageDimensions;

// camera type
// ...

void main () {
	const int index = int( gl_GlobalInvocationID.x );
	ivec2 offset = ivec2( offsets[ index ] );
	seed = offset.x * 100625 + offset.y * 2324 + gl_GlobalInvocationID.x * 235;

	// generate initial ray origins + directions... very basic camera logic
	vec2 uv = ( vec2( offset ) + vec2( NormalizedRandomFloat(), NormalizedRandomFloat() ) ) / imageDimensions;
	uv = uv * 2.0f - vec2( 1.0f );

	const float aspectRatio = imageDimensions.x / imageDimensions.y;

	// zero out the buffer entry
	StateReset( state[ index ] );

	// filling out the rayState_t struct
	SetRayOrigin( state[ index ], viewerPosition );
	// SetRayDirection( state[ index ], normalize( aspectRatio * uv.x * basisX + uv.y * basisY + ( 1.0f / ( FoV + 0.025f * ( 1.0f - sqrt( NormalizedRandomFloat() ) ) ) ) * basisZ ) ); // interesting motion blur ish thing from jittering FoV
	SetRayDirection( state[ index ], normalize( aspectRatio * uv.x * basisX + uv.y * basisY + ( 1.0f / FoV ) * basisZ ) );
	SetPixelIndex( state[ index ], offset );
}
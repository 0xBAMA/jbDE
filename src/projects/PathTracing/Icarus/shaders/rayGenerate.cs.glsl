#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;
//=============================================================================================================================
#include "random.h"
#include "rayState.h.glsl"
#include "biasGain.h"
//=============================================================================================================================
// pixel offset + ray state buffers
layout( binding = 0, std430 ) readonly buffer pixelOffsets { uvec2 offsets[]; };
layout( binding = 1, std430 ) buffer rayState { rayState_t state[]; };
//=============================================================================================================================
layout( location = 0, rgba8ui ) readonly uniform uimage2D blueNoise;
vec4 Blue( in ivec2 loc ) { return imageLoad( blueNoise, ( loc + noiseOffset ) % imageSize( blueNoise ).xy ); }
//=============================================================================================================================
uniform vec3 basisX;
uniform vec3 basisY;
uniform vec3 basisZ;
uniform vec3 viewerPosition;
uniform vec2 imageDimensions;
//=============================================================================================================================
uniform float FoV;
uniform int cameraType;
// chromab toggle
// UV scale factor
// Voraldo perspective factor
// DoF toggle
// DoF bokeh mode
// DoF focus distance
// DoF jitter radius
// ...
//=============================================================================================================================
struct cameraRay {
	vec3 origin;		// initial ray source position (jittered)
	vec3 direction;		// initial ray direction
	vec3 transmission;	// for the chromatic aberration, if desired
};
//=============================================================================================================================
// camera types
#define DEFAULT		0
#define ORTHO		1
#define SPHERICAL	2
#define HDRI		3
#define COMPOUND	4
#define VORALDO		5
//=============================================================================================================================
cameraRay GetCameraRay ( vec2 uv ) {
	cameraRay temp;

	const float aspectRatio = imageDimensions.x / imageDimensions.y;
	uv.x = uv.x * aspectRatio;

	// compute FoV offset
		// this also informs initial transmission values

	// use offset FoV to calculate camera ray direction...
	switch ( cameraType ) {
		case DEFAULT: {

			break;
		}

		case ORTHO: {

			break;
		}

		case SPHERICAL: {

			break;
		}

		case HDRI: {

			break;
		}

		case COMPOUND: {

			break;
		}

		case VORALDO: {

			break;
		}

		default: // shouldn't hit this
		break;
	}

	// jittering the ray origin helps with some aliasing issues
	r.origin += ( Blue( ivec2( gl_GlobalInvocationID.xy ) ).z / 255.0f ) * 0.1f * r.direction;

	// if the DoF is on, we do some additional calculation...
	if ( DoFEnable ) {
		// thin lens adjustment
		vec3 focuspoint = temp.origin + ( ( temp.direction * DoFFocusDistance ) / dot( temp.direction, basisZ ) );
		vec2 diskOffset = DoFRadius * GetBokehOffset( DoFBokehMode );
		temp.origin += diskOffset.x * basisX + diskOffset.y * basisY;
		temp.direction = normalize( focuspoint - temp.origin );
	}

	return temp;
}
//=============================================================================================================================
void main () {
	const int index = int( gl_GlobalInvocationID.x );
	ivec2 offset = ivec2( offsets[ index ] );
	seed = offset.x * 100625 + offset.y * 2324 + gl_GlobalInvocationID.x * 42069;

	// generate initial ray origins + directions... very basic camera logic
	vec2 uv = ( ( vec2( offset ) + vec2( NormalizedRandomFloat(), NormalizedRandomFloat() ) ) / imageDimensions ) * 2.0f - vec2( 1.0f );

	// zero out the buffer entry
	StateReset( state[ index ] );

	#if 0 // interesting motion blur ish thing from jittering FoV
		const float interpolationValue = NormalizedRandomFloat();
		const float i = gainValue( interpolationValue, 0.7f ) * ( 3.1415f / 2.0f );
		const vec3 colorWeight = vec3( sin( i ), sin( i * 2.0f ), cos( i ) ) * 1.618f;
		// convolving transmission with the offset... gives like chromatic aberration
		SetTransmission( state[ index ], pow( colorWeight, vec3( 1.0f / 2.2f ) ) );
		SetRayDirection( state[ index ], normalize( uv.x * basisX + uv.y * basisY + ( 1.0f / ( FoV + 0.006f * gainValue( interpolationValue, 0.75f ) ) ) * basisZ ) );
	#else
		SetRayDirection( state[ index ], normalize( uv.x * basisX + uv.y * basisY + ( 1.0f / FoV ) * basisZ ) );
	#endif

	// filling out the rayState_t struct
	SetRayOrigin( state[ index ], viewerPosition );
	SetPixelIndex( state[ index ], offset );
}
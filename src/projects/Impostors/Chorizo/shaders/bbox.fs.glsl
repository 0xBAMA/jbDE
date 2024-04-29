#version 430

in flat uint vofiIndex;
in vec3 vofiPosition;
in vec4 vofiColor;
out vec4 outColor;

uniform vec3 eyePosition;	// location of the viewer
uniform float numPrimitives;

#include "colorRamps.glsl.h"
#include "consistentPrimitives.glsl.h"

// ===================================================================================================
uniform mat4 viewTransform;
// ===================================================================================================
struct transform_t {
	mat4 transform;
	mat4 inverseTransform;
};
layout( binding = 0, std430 ) buffer transformsBuffer {
	transform_t transforms[];
};

// ===================================================================================================
struct parameters_t {
	float data[ 16 ];
};
layout( binding = 1, std430 ) buffer parametersBuffer {
	parameters_t parametersList[];
};

// TAA resources:
// https://ziyadbarakat.wordpress.com/2020/07/28/temporal-anti-aliasing-step-by-step/
// https://sugulee.wordpress.com/2021/06/21/temporal-anti-aliasingtaa-tutorial/

out float gl_FragDepth;
layout( location = 0 ) out vec4 normalResult;
layout( location = 1 ) out uvec4 primitiveID;

// visalizing the fragments that get drawn
#define SHOWDISCARDS

void main() {
	const mat4 transform = transforms[ vofiIndex ].transform;
	const mat4 inverseTransform = transforms[ vofiIndex ].inverseTransform;

	// eye parameter
	const vec3 eyeVectorToFragment = vofiPosition - eyePosition;
	const vec3 rayOrigin = eyePosition;
	const vec3 rayDirection = normalize( eyeVectorToFragment );

	// this assumes that the contained primitive is located at the origin of the box
	// const vec3 centerPoint = ( inverseTransform * vec4( vec3( 0.0f ), 0.0f ) ).xyz;
	// float result = iSphere( rayOrigin - centerPoint, rayDirection, normal, 1.0f );
	// float result = iCapsule( rayOrigin - centerPoint, rayDirection, normal, vec3( -0.7f ), vec3( 0.7f ), 0.25f );
	// float result = iRoundedCone( rayOrigin - centerPoint, rayDirection, normal, vec3( -0.9f ), vec3( 0.7f ), 0.1f, 0.3f );
	// float result = iRoundedBox( rayOrigin - centerPoint, rayDirection, normal, vec3( 0.9f ), 0.1f );

	// intersecting with the contained primitive
	parameters_t parameters = parametersList[ vofiIndex ];
	vec3 normal = vec3( 0.0f );
	float result = iCapsule( rayOrigin, rayDirection, normal,
		vec3( parameters.data[ 1 ], parameters.data[ 2 ], parameters.data[ 3 ] ), // point A
		vec3( parameters.data[ 5 ], parameters.data[ 6 ], parameters.data[ 7 ] ), // point B
		parameters.data[ 4 ] ); // radius


	if ( result == MAX_DIST ) { // miss condition
		#ifdef SHOWDISCARDS
			if ( ( int( gl_FragCoord.x ) % 2 == 0 ) ) {
				outColor = vec4( 1.0f ); // placeholder, helps visualize bounding box
				primitiveID = uvec4( 0xffff );
			} else {
		#endif
				discard;
		#ifdef SHOWDISCARDS
			}
		#endif
	} else {

		// rasterizer outputs
		primitiveID = uvec4( vofiIndex, 0, 0, 0 );
		normalResult = vec4( normal, 1.0f );

		// writing correct depths
		const vec4 projectedPosition = viewTransform * vec4( rayOrigin + result * rayDirection, 1.0f );
		gl_FragDepth = ( projectedPosition.z / projectedPosition.w + 1.0f ) * 0.5f;

	}
}
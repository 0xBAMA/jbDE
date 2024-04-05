#version 430

in flat uint vofiIndex;
in vec3 vofiPosition;
in vec4 vofiColor;
out vec4 outColor;

uniform vec3 eyePosition;	// location of the viewer
uniform float numPrimitives;

#include "colorRamps.glsl.h"
#include "consistentPrimitives.glsl.h"

uniform mat4 viewTransform;
struct transform_t {
	mat4 transform;
	mat4 inverseTransform;
};
layout( binding = 0, std430 ) buffer transformsBuffer {
	transform_t transforms[];
};

out float gl_FragDepth;
layout( location = 0 ) out vec4 normalResult;
layout( location = 1 ) out uvec4 materialID;

// visalizing the fragments that get drawn
// #define SHOWDISCARDS

void main() {
	const mat4 transform = transforms[ vofiIndex ].transform;
	const mat4 inverseTransform = transforms[ vofiIndex ].inverseTransform;

	// transform the view ray, origin + direction, into "primitive space"
	const vec3 eyeVectorToFragment = vofiPosition - eyePosition;
	const vec3 rayOrigin = ( inverseTransform * vec4( eyePosition, 1.0f ) ).xyz;
	const vec3 rayDirection = normalize( inverseTransform * vec4( eyeVectorToFragment, 0.0f ) ).xyz;
	const vec3 centerPoint = ( inverseTransform * vec4( vec3( 0.0f ), 0.0f ) ).xyz;

	// this assumes that the contained primitive is located at the origin of the box
	vec3 normal = vec3( 0.0f );
	// float result = iSphere( rayOrigin - centerPoint, rayDirection, normal, 1.0f );
	// float result = iCapsule( rayOrigin - centerPoint, rayDirection, normal, vec3( -0.7f ), vec3( 0.7f ), 0.25f );
	// float result = iRoundedCone( rayOrigin - centerPoint, rayDirection, normal, vec3( -0.9f ), vec3( 0.7f ), 0.1f, 0.3f );
	float result = iRoundedBox( rayOrigin - centerPoint, rayDirection, normal, vec3( 0.9f ), 0.1f );

	if ( result == MAX_DIST ) { // miss condition
		#ifdef SHOWDISCARDS
			if ( ( int( gl_FragCoord.x ) % 2 == 0 ) ) {
				outColor = vec4( 1.0f ); // placeholder, helps visualize bounding box
			} else {
		#endif
				discard;
		#ifdef SHOWDISCARDS
			}
		#endif
	} else {
		const vec3 hitVector = rayOrigin + result * rayDirection;
		const vec3 hitPosition = ( transform * vec4( hitVector, 1.0f ) ).xyz;
		const vec3 transformedNormal = normalize( ( transform * vec4( normal, 0.0f ) ).xyz );

		materialID = uvec4( vofiIndex, 0, 0, 0 );
		normalResult = vec4( transformedNormal, 1.0f );

		// shading
		outColor.xyz = refPalette( float( vofiIndex ) / numPrimitives, INFERNO ).xyz * 0.25f;
		outColor.xyz += 0.25f * clamp( dot( transformedNormal, normalize( vec3( 1.0f ) ) ), 0.0f, 1.0f );
		outColor.a = 1.0f;

		vec4 projectedPosition = viewTransform * vec4( hitPosition, 1.0f );
		gl_FragDepth = ( projectedPosition.z / projectedPosition.w + 1.0f ) * 0.5f;
	}
}
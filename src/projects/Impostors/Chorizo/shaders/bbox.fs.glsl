#version 430

in flat uint vofiIndex;
in vec3 vofiPosition;
in vec4 vofiColor;
out vec4 outColor;

// location of the viewer
uniform vec3 eyePosition;

// https://iquilezles.org/articles/intersectors/ some good resources here

vec2 RaySphereIntersect ( vec3 r0, vec3 rd, vec3 s0, float sr ) {
	// r0 is ray origin
	// rd is ray direction
	// s0 is sphere center
	// sr is sphere radius
	// return is the roots of the quadratic, includes negative hits
	float a = dot( rd, rd );
	vec3 s0_r0 = r0 - s0;
	float b = 2.0f * dot( rd, s0_r0 );
	float c = dot( s0_r0, s0_r0 ) - ( sr * sr );
	float disc = b * b - 4.0f * a * c;
	if ( disc < 0.0f ) {
		return vec2( -1.0f, -1.0f );
	} else {
		return vec2( -b - sqrt( disc ), -b + sqrt( disc ) ) / ( 2.0f * a );
	}
}

uniform mat4 viewTransform;
layout( binding = 0, std430 ) buffer transformsBuffer {
	mat4 transforms[];
};

void main() {
	// how do we do the math here?
		// transform the ray, origin + direction, into "primitive space"

	// this assumes that the sphere is located at the origin
	const mat4 transform = transforms[ vofiIndex ];
	const mat4 inverseTransform = inverse( transform );
	const vec3 eyeVectorToFragment = vofiPosition - eyePosition;
	const vec3 rayOrigin = ( inverseTransform * vec4( eyePosition, 1.0f ) ).xyz;
	const vec3 rayDirection = ( inverseTransform * vec4( eyeVectorToFragment, 0.0f ) ).xyz;
	const vec3 sphereCenter = ( inverseTransform * vec4( vec3( 0.0f ), 0.0f ) ).xyz;

	vec2 result = RaySphereIntersect( rayOrigin, rayDirection, sphereCenter, 1.0f );
	if ( result == vec2( -1.0f, -1.0f ) ) { // miss condition
		// if ( ( int( gl_FragCoord.x ) % 2 == 0 ) ) {
			// outColor = vec4( 1.0f ); // placeholder, helps visualize bounding box
		// } else {
			discard;
		// }
	} else {
		const vec3 hitVector = rayOrigin + result.x * rayDirection;
		const vec3 hitPosition = ( transform * vec4( hitVector, 1.0f ) ).xyz;
		outColor = vec4( hitPosition, 1.0f );

		// fractional depth... need to figure out a more consistent way to handle this
		gl_FragDepth = gl_FragCoord.z;
		
		// these are fucked
		// const float depthOffset = length( eyeVectorToFragment - hitVector ) / 100.0f;
		// const float depthOffset = length( hitPosition - vofiPosition ) / 100.0f;
		// gl_FragDepth = gl_FragCoord.z + depthOffset;
	}
}
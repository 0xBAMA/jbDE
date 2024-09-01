#version 430 core
layout( local_size_x = 1024, local_size_y = 1, local_size_z = 1 ) in;

layout( r32ui ) uniform uimage3D current;
layout( rgba32f ) uniform image3D simData;

struct agent_t {

	// location, extra float available
	vec4 position;

	// direction now needs to specify the basis vectors to construct sample positions
	vec4 basisX;
	vec4 basisY;
	vec4 basisZ;

	// other info? parameters per-agent?

};

layout( binding = 0, std430 ) buffer agentData {
	agent_t data[];
};

uniform float stepSize;
uniform float senseAngle;
uniform float senseDistance;
uniform float turnAngle;
uniform uint depositAmount;
uniform uint numAgents;
uniform bool writeBack;

#include "hg_sdf.glsl"

// takes argument in radians
vec2 rotate ( const vec2 v, const float a ) {
	const float s = sin( a );
	const float c = cos( a );
	const mat2 m = mat2( c, -s, s, c );
	return m * v;
}

mat3 Rotate3D ( const float angle, const vec3 axis ) {
	const vec3 a = normalize( axis );
	const float s = sin( angle );
	const float c = cos( angle );
	const float r = 1.0f - c;
	return mat3(
		a.x * a.x * r + c,
		a.y * a.x * r + a.z * s,
		a.z * a.x * r - a.y * s,
		a.x * a.y * r - a.z * s,
		a.y * a.y * r + c,
		a.z * a.y * r + a.x * s,
		a.x * a.z * r + a.y * s,
		a.y * a.z * r - a.x * s,
		a.z * a.z * r + c
	);
}

vec3 wrapPosition ( vec3 pos ) {
	if ( pos.x >=  1.0f ) pos.x -= 2.0f;
	if ( pos.x <= -1.0f ) pos.x += 2.0f;
	if ( pos.y >=  1.0f ) pos.y -= 2.0f;
	if ( pos.y <= -1.0f ) pos.y += 2.0f;
	if ( pos.z >=  1.0f ) pos.z -= 2.0f;
	if ( pos.z <= -1.0f ) pos.z += 2.0f;
	return pos;
}

// random utilites
uniform int wangSeed;
#include "random.h"

void main () {

	const uvec3 globalDims = gl_NumWorkGroups * gl_WorkGroupSize;
	const int index = int(
		gl_GlobalInvocationID.z * globalDims.x * globalDims.y +
		gl_GlobalInvocationID.y * globalDims.x +
		gl_GlobalInvocationID.x );

	if ( index < numAgents ) {
		seed = wangSeed + 69 * index; // initialize the rng
		agent_t a = data[ index ];

		vec3 forwards = a.basisZ.xyz;
		vec3 tippedUp = Rotate3D( senseAngle, a.basisX.xyz ) * forwards;

		vec3 weightedSum = vec3( 0.0f );
		const int numDirections = 3;
		uint samples[ numDirections ];
		for ( int i = 0; i < numDirections; i++ ) {
			mat3 rollMat = Rotate3D( TAU * ( float( i ) / float( numDirections ) ), a.basisZ.xyz );
			vec3 rotated = rollMat * tippedUp;
			vec3 flatSpace = Rotate3D( TAU * ( float( i ) / float( numDirections ) ), vec3( 0.0f, 0.0f, 1.0f ) ) * vec3( 0.0f, 1.0f, 0.0f );
			samples[ i ] = imageLoad( current, ivec3( imageSize( current ) * wrapPosition( 0.5f * ( a.position.xyz + senseDistance * rotated + vec3( 1.0f ) ) ) ) ).r;
			weightedSum += float( samples[ i ] ) * flatSpace; // this can become 2d
		}


		// basically need like, angle about the forward z axis, from the weighted sum
			// ... if within some small circle of the forward z axis ( dot product > thresh ~0.9 or something like that, xy plane offset smaller than some value ), that's when we do the random direction logic - will also need to create a new basis, in that case - for now, that case will just be, no-op
		
		mat3 updateRotation = mat3( 1.0f );
		if ( length( weightedSum.xy ) < 0.1f ) {
			// this is the random direction behavior
			for ( int i = 0; i < 3; i++ ) {
				updateRotation *= Rotate3D( NormalizedRandomFloat(), RandomUnitVector() );
			}
		} else {
			float angle = atan( weightedSum.y, weightedSum.x );
			// tilt up by the turn angle, then roll by the solved-for angle
			updateRotation = Rotate3D( angle, a.basisZ.xyz ) * Rotate3D( turnAngle, a.basisX.xyz );
		}

		a.basisX.xyz = updateRotation * a.basisX.xyz;
		a.basisY.xyz = updateRotation * a.basisY.xyz;
		a.basisZ.xyz = updateRotation * a.basisZ.xyz;

		if ( writeBack ) { // solve for the new direction, update the basis vectors to indicate
			data[ index ].basisX.xyz = a.basisX.xyz;
			data[ index ].basisY.xyz = a.basisY.xyz;
			data[ index ].basisZ.xyz = a.basisZ.xyz;
		}

		vec3 newPosition = wrapPosition( a.position.xyz + stepSize * a.basisZ.xyz );
		data[ index ].position.xyz = newPosition;

		imageAtomicAdd( current, ivec3( imageSize( current ) * ( 0.5f * ( newPosition + vec3( 1.0f ) ) ) ), depositAmount );
	}
}

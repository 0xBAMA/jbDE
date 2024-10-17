#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;

#include "random.h"
#include "rayState.h.glsl"

// pixel offset + ray state buffers
layout( binding = 0, std430 ) readonly buffer pixelOffsets { uvec2 offsets[]; };
layout( binding = 1, std430 ) buffer rayState { rayState_t state[]; };


// basic raymarch stuff
#define rot(a) mat2(cos(a),sin(a),-sin(a),cos(a))
float de(vec3 p){
	for(int j=0;++j<8;)
		p.z-=.3,
		p.xz=abs(p.xz),
		p.xz=(p.z>p.x)?p.zx:p.xz,
		p.xy=(p.y>p.x)?p.yx:p.xy,
		p.z=1.-abs(p.z-1.),
		p=p*3.-vec3(10,4,2);
	return length(p)/6e3-.001;
}

const float raymarchMaxDistance = 100.0f;
const float raymarchUnderstep = 0.9f;
const int raymarchMaxSteps = 300;
const float epsilon = 0.001f;

float raymarch ( vec3 rayOrigin, vec3 rayDirection ) {
	float dQuery = 0.0f;
	float dTotal = 0.0f;
	vec3 pQuery = rayOrigin;
	for ( int steps = 0; steps < raymarchMaxSteps; steps++ ) {
		pQuery = rayOrigin + dTotal * rayDirection;
		dQuery = de( pQuery );
		dTotal += dQuery * raymarchUnderstep;
		if ( dTotal > raymarchMaxDistance || abs( dQuery ) < epsilon ) {
			break;
		}
	}
	return dTotal;
}

vec3 SDFNormal( in vec3 position ) {
	vec2 e = vec2( epsilon, 0.0f );
	return normalize( vec3( de( position ) ) - vec3( de( position - e.xyy ), de( position - e.yxy ), de( position - e.yyx ) ) );
}

void main () {

	rayState_t myRay = state[ gl_GlobalInvocationID.x ];
	seed = uint( GetPixelIndex( myRay ).x ) * 100625 + uint( GetPixelIndex( myRay ).y ) * 2324 + gl_GlobalInvocationID.x * 235;

	// do the raymarch...
	const vec3 origin = GetRayOrigin( myRay );
	const vec3 direction = GetRayDirection( myRay );
	const float distanceToHit = raymarch( origin, direction );

	// update the intersection info
	SetHitDistance( state[ gl_GlobalInvocationID.x ], distanceToHit );
	SetHitIntersector( state[ gl_GlobalInvocationID.x ], ( distanceToHit < raymarchMaxDistance ) ? 1 : 0 );
	SetHitNormal( state[ gl_GlobalInvocationID.x ], SDFNormal( origin + direction * distanceToHit ) );
}
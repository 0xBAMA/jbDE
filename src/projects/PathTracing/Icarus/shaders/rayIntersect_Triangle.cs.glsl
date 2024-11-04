#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;

// ray state buffer
#include "rayState.h.glsl"
layout( binding = 1, std430 ) buffer rayState { rayState_t state[]; };

#include "random.h"

vec4 triangle(vec3 o, vec3 d, vec3 v0, vec3 v1, vec3 v2) {
  // find a,b such that:
  // p + kr = av0 + bv1 + (1-a-b)v2 = a(v0-v2)+b(v1-v2)+v2
  // ie. -kr + a(v0-v2) + b(v1-v2) = p-v2
  vec3 e1 = v0 - v2;
  vec3 e2 = v1 - v2;
  vec3 t = o - v2;
  vec3 p = cross(d,e2);
  vec3 q = cross(t,e1);
  vec3 a = vec3(dot(q,e2),dot(p,t),dot(q,d))/dot(p,e1);
  return vec4(a,1.0-a.y-a.z);
}

const vec3 carrot = vec3( 0.713f, 0.170f, 0.026f );
const vec3 honey = vec3( 0.831f, 0.397f, 0.038f );
const vec3 bone = vec3( 0.887f, 0.789f, 0.434f );
const vec3 tire = vec3( 0.023f, 0.023f, 0.023f );
const vec3 sapphire = vec3( 0.670f, 0.764f, 0.855f );
const vec3 nickel = vec3( 0.649f, 0.610f, 0.541f );

void main () {
	rayState_t myState = state[ gl_GlobalInvocationID.x ];

	const uvec2 loc = uvec2( GetPixelIndex( myState ) );
	seed = loc.x * 10625 + loc.y * 23624 + gl_GlobalInvocationID.x * 2335;

	const vec3 origin = GetRayOrigin( myState );
	const vec3 direction = GetRayDirection( myState );

	// ray is live, do the raymarch...
	if ( length( direction ) > 0.5f ) {
		// update the intersection info
		const float scale = 2.2f;
		const vec3 offset = vec3( 0.0f, 0.4f, 0.0f );
		const vec3 p0 = scale * vec3(  0.0f, 0.0f,  0.5f ).xzy + offset;
		const vec3 p1 = scale * vec3(  0.5f, 0.0f, -0.5f ).xzy + offset;
		const vec3 p2 = scale * vec3( -0.5f, 0.0f, -0.5f ).xzy + offset;

		const vec4 result = triangle( origin, direction, p0, p1, p2 );
		bool hit = all( bvec2( all( greaterThanEqual( result, vec4( 0.0f ) ) ), all( lessThanEqual( result.yzw, vec3( 1.0f ) ) ) ) );

		if ( hit && result.x < GetHitDistance( myState ) ) {
			SetHitAlbedo( myState, result.yzw * 1.0f );
			// SetHitAlbedo( myState, ( result.y * honey + result.z * carrot + result.w * sapphire ) * 4.0f );
			// SetHitAlbedo( myState, vec3( 4.0f ) );
			SetHitRoughness( myState, 0.0f );
			SetHitDistance( myState, result.x );
			SetHitMaterial( myState, EMISSIVE );

			const vec3 normal = normalize( cross( p0 - p1, p0 - p2 ) );
			SetHitNormal( myState, ( dot( direction, normal ) > 0 ) ? normal : -normal );
			SetHitIntersector( myState, TRIANGLEHIT );

			state[ gl_GlobalInvocationID.x ] = myState;
		}
	}
}
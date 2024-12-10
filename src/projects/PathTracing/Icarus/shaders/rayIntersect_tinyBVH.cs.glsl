#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;
//=============================================================================================================================
#include "random.h"
#include "pbrConstants.glsl"
#include "rayState2.h.glsl"
//=============================================================================================================================
layout( binding = 1, std430 ) readonly buffer rayState { rayState_t state[]; };
layout( binding = 2, std430 ) writeonly buffer intersectionBuffer { intersection_t intersectionScratch[]; };
//=============================================================================================================================
// Model intersection - placeholder
	// buffer setup
bool intersect ( rayState_t rayState ) { return false; }
//=============================================================================================================================
void main () {
	const rayState_t myState = state[ gl_GlobalInvocationID.x ];

	// seed the RNG
	const uvec2 loc = uvec2( GetPixelIndex( myState ) );
	seed = loc.x * 10625 + loc.y * 23624 + gl_GlobalInvocationID.x * 42069;

	// ray living check
	if ( IsLiving( myState ) ) {

		// do the intersection operation
		bool hit = intersect( myState );

		// fill out the intersection struct
		intersection_t TinyBVHIntersection;
		IntersectionReset( TinyBVHIntersection );

		// // set color based on barycentrics
		// SetHitAlbedo( TinyBVHIntersection, result.yzw * 3.0f );
		// SetHitRoughness( TinyBVHIntersection, 0.0f );
		// SetHitDistance( TinyBVHIntersection, result.x );
		// SetHitMaterial( TinyBVHIntersection, EMISSIVE );

		// const vec3 normal = normalize( cross( p0 - p1, p0 - p2 ) );
		// SetHitFrontface( TinyBVHIntersection, ( dot( GetRayDirection( myState ), normal ) > 0 ) );
		// SetHitNormal( TinyBVHIntersection, GetHitFrontface( TinyBVHIntersection ) ? normal : -normal );
		// SetHitIntersector( TinyBVHIntersection, TRIANGLEHIT );

		// write it back to the buffer
		if ( hit ) {
			const int index = int( gl_GlobalInvocationID.x ) * NUM_INTERSECTORS + INTERSECT_TINYBVH;
			intersectionScratch[ index ] = TinyBVHIntersection;
		}
	}
}
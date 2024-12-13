#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;
//=============================================================================================================================
#include "random.h"
#include "noise.h"
#include "hg_sdf.glsl"
#include "pbrConstants.glsl"
#include "rayState2.h.glsl"
#include "wood.h"
//=============================================================================================================================
layout( binding = 1, std430 ) readonly buffer rayState { rayState_t state[]; };
layout( binding = 2, std430 ) writeonly buffer intersectionBuffer { intersection_t intersectionScratch[]; };
//=============================================================================================================================
mat2 rot2(in float a){ float c = cos(a), s = sin(a); return mat2(c, s, -s, c); }
float deGyroid ( vec3 p ) {
	float d = 1e5;
	const int n = 3;
	const float fn = float(n);
	for(int i = 0; i < n; i++){
		vec3 q = p;
		float a = float(i)*fn*2.422; //*6.283/fn
		a *= a;
		q.z += float(i)*float(i)*1.67; //*3./fn
		q.xy *= rot2(a);
		float b = (length(length(sin(q.xy) + cos(q.yz))) - .15);
		float f = max(0., 1. - abs(b - d));
		d = min(d, b) - .25*f*f;
	}
	return d;
}
  #define fold45(p)(p.y>p.x)?p.yx:p
  float deTemple(vec3 p) {
    float scale = 2.1, off0 = .8, off1 = .3, off2 = .83;
    vec3 off =vec3(2.,.2,.1);
    float s=1.0;
    for(int i = 0;++i<20;) {
      p.xy = abs(p.xy);
      p.xy = fold45(p.xy);
      p.y -= off0;
      p.y = -abs(p.y);
      p.y += off0;
      p.x += off1;
      p.xz = fold45(p.xz);
      p.x -= off2;
      p.xz = fold45(p.xz);
      p.x += off1;
      p -= off;
      p *= scale;
      p += off;
      s *= scale;
    }
    return length(p)/s;
  }

//=============================================================================================================================
const float scaleFactor = 100.0f;
// layout( rgba8ui ) readonly uniform uimage3D VoraldoModel;
// layout( r32f ) readonly uniform image2D HeightmapTex;
//=============================================================================================================================
vec3 scatterColor;
float GetVolumeDensity( ivec3 pos ) {

	float density;
	// pos = pos + ivec3( 100 );

	// const ivec3 iS = imageSize( VoraldoModel );
	// if ( any( greaterThanEqual( pos, iS ) ) || any( lessThan( pos, ivec3( 0 ) ) ) ) {
		// out of bounds
		// scatterColor = sapphire;
		// density = 10.0f;
	// } else {
	// 	uvec4 VoraldoSample = imageLoad( VoraldoModel, pos );
	// 	// scatterColor = vec3( VoraldoSample.xyz ) / 255.0f;
	// 	// scatterColor = vec3( VoraldoSample.xyz );
	// 	scatterColor = vec3( 0.618f );
	// 	// density = pow( VoraldoSample.w / 255.0f, 2.0f ) * 2.0f;
	// 	density = 20.0f + VoraldoSample.a;
	// }

	vec3 p = pos / scaleFactor;
	vec3 pOriginal = p;

	// const float scale = 0.1618f;
	// bool blackOrWhite = ( step( 0.0f,
	// 	cos( scale * pi * p.x + pi / 2.0f ) *
	// 	cos( scale * pi * p.y + pi / 2.0f ) *
	// 	cos( scale * pi * p.z + pi / 2.0f ) ) == 0 );

	const vec3 bboxSize = vec3( 5.0f, 5.0f, 22.0f );

	// // pModInterval1( p.z, 3.0f, -6.0f, 6.0f );
	// // pModInterval1( p.x, 3.0f, -6.0f, 6.0f );
	// // pModInterval1( p.y, 3.0f, -6.0f, 6.0f );
	// // const float dCutout = distance( p, vec3( 0.0f, 0.0f, 0.0f ) ) - 1.618f;
	// // p = pOriginal;

	// const float dCutout = deTemple( p * 3.0f );

	// const float scalar = 0.8f;
	// // float d = max( deWhorl( p / 1.0f ), fPlane( p, vec3( 1.0f, 0.0f, 0.0f ), 0.0f ) );
	// // const float boxDist = fBox( p, vec3( 10.0f, 3.0f, 12.0f ) );
	// // const float boxDist = fBox( p, vec3( 15.0f ) );

	const float bboxDist = fBox( p, bboxSize );
	// const float boxDist = max( bboxDist, -dCutout );
	// float value = 0.0f;

	// float d = max( deGyroid( p * scalar ) / scalar, boxDist ) - ( blackOrWhite ? -0.02f : 0.05f );
	// if ( d < 0.0f ) {
	// 	value = 200000.0f * -d;
	// 	// scatterColor = vec3( 0.99f );
	// 	scatterColor = bone.brg;
	// } else if ( boxDist < 0.0f ) {
	// if ( bboxDist < 0.0f ) {
	// 	// value = 1.0f;
	// 	// value = 2.618f;
	// 	// value = 150.0f;
	// 	value = 5.0f;
	// 	scatterColor = sapphire;

	// 	// const vec3 woodColor = matWood( p * 0.3f );
	// 	// if ( boxDist < 0.0f ) {
	// 	// 	value = 5000.0f * GetLuma( woodColor ).r;
	// 	// 	// value = 2000.0f * ( 1.0f - GetLuma( woodColor ).r );
	// 	// 	// scatterColor = vec3( woodColor.bgr );
	// 	// 	scatterColor = mix( vec3( 1.0f ), sapphire, 1.0f - woodColor.r );
	// 	// } else {
	// 	// 	value = blackOrWhite ? 400.0f : 30.0f;
	// 	// 	scatterColor = blackOrWhite ? cobalt : mix( carrot.brg, honey.brg, 1.0f - sqrt( woodColor.r ) );
	// 	// }

	// 	// pMod2( p.xy, vec2( 0.5f ) );
	// 	// if ( p.x < 0.1 && p.y < 0.15f ) {
	// 		// scatterColor = mix( blood, sapphire, 0.618f );
	// 		// value = 800.0f;
	// 	// }

	// 	// scatterColor = vec3( 0.618f );
	// 	// scatterColor = sapphire;
	// 	// scatterColor = copper;
	
	
	// // } else if ( fBox( p, bboxSize * 1.25f ) < 0.0f ) {
	// // 	// a bit bigger box
	// // 	value = 6.18f;
	// // 	scatterColor = vec3( 0.9f );
	// } else {
	// 	value = 0.0f;
	// }

	if ( bboxDist < 0.0f ) {
		scatterColor = sapphire;
		density = 2.0f;
	} else {
		density = 0.0f;
	}


	return density;
}
//=============================================================================================================================
void main () {
	const rayState_t myState = state[ gl_GlobalInvocationID.x ];

	const uvec2 loc = uvec2( GetPixelIndex( myState ) );
	seed = loc.x * 10625 + loc.y * 23624 + gl_GlobalInvocationID.x * 2335;

	const vec3 origin = GetRayOrigin( myState );
	const vec3 direction = GetRayDirection( myState );

	if ( IsLiving( myState ) ) {

		// DDA traversal to a potential scattering event
		bool hit = false;
		float hitDistance = MAX_DIST;
		vec3 transmission = vec3( 0.0f );
		const vec3 startPos = origin * scaleFactor;

		// start the DDA traversal...
		// from https://www.shadertoy.com/view/7sdSzH
		vec3 deltaDist = 1.0f / abs( direction );
		ivec3 rayStep = ivec3( sign( direction ) );
		bvec3 mask0 = bvec3( false );
		ivec3 mapPos0 = ivec3( floor( startPos ) );
		vec3 sideDist0 = ( sign( direction ) * ( vec3( mapPos0 ) - startPos ) + ( sign( direction ) * 0.5f ) + 0.5f ) * deltaDist;

		#define MAX_RAY_STEPS 3000
		for ( int i = 0; i < MAX_RAY_STEPS && !hit; i++ ) {

			bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
			vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
			ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

			// doing Beer's law - if we pass a threshold check, we scatter
			// int densityRead = max( 0, int( imageLoad( continuum, mapPos0 ).r ) - noiseFloor );
			float densityRead = GetVolumeDensity( mapPos0 ); // constant density for now
			int densityThreshold = 5000;
			float density = exp( -densityRead / float( densityThreshold ) );

			if ( NormalizedRandomFloat() > density ) {

			// materials stuff:
				// // ============================================================================
				// // transmission attenuates by the density
				// 	transmission *= saturate( density );
				transmission = saturate( density ) * scatterColor;
				// transmission = 0.99f;
				// // ============================================================================
				// // direction - positive rdWeight means forward scatter, 0 is random scatter, negative is backscatter
				// 	rayDirection = normalize( viewerScatterWeight * rayDirection + RandomUnitVector() );
				// // ============================================================================

				hit = true;
				hitDistance = length( ivec3( floor( startPos ) ) - mapPos0 ) / scaleFactor;

			} else {
				// take a regular step
				sideDist0 = sideDist1;
				mapPos0 = mapPos1;
			}
		}

		// fill out intersection struct
		intersection_t VolumeIntersection;
		IntersectionReset( VolumeIntersection );

		SetHitAlbedo( VolumeIntersection, transmission );
		SetHitRoughness( VolumeIntersection, 0.0f );
		SetHitDistance( VolumeIntersection, hitDistance );

		// SetHitMaterial( VolumeIntersection, VOLUME ); // this doesn't really mean anything (should it?)
		// SetHitNormal( VolumeIntersection, ... ); // normal doesn't really mean anything here
		SetHitIntersector( VolumeIntersection, VOLUMEHIT ); // this is the key delineation... material seems better, tbd

		if ( hit ) { // this ray hits in the specified range, write back to the buffer
			const int index = int( gl_GlobalInvocationID.x ) * NUM_INTERSECTORS + INTERSECT_VOLUME;
			intersectionScratch[ index ] = VolumeIntersection;
		}
	}
}
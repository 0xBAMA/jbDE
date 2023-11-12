#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// util + output
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// results of the precompute step
layout( binding = 2, rg32ui ) uniform uimage3D idxBuffer;
layout( binding = 3, rgba16f ) uniform image3D colorBuffer;

// forward pathtracing buffers
layout( r32ui ) uniform uimage3D redAtomic;
layout( r32ui ) uniform uimage3D greenAtomic;
layout( r32ui ) uniform uimage3D blueAtomic;
layout( r32ui ) uniform uimage3D countAtomic;

// sphere data
const int numSpheres = 65535;
struct sphere_t {
	vec4 positionRadius;
	vec4 colorMaterial;
};
layout( binding = 0, std430 ) buffer sphereData {
	sphere_t spheres[];
};

uniform ivec2 noiseOffset;
uvec4 blueNoiseRead ( ivec2 loc ) {
	ivec2 wrappedLoc = ( loc + noiseOffset ) % imageSize( blueNoiseTexture );
	uvec4 sampleValue = imageLoad( blueNoiseTexture, wrappedLoc );
	return sampleValue;
}

float blueNoiseRead ( ivec2 loc, int idx ) {
	vec4 sampleValue = blueNoiseRead( loc ) / 255.0f;
	switch ( idx ) {
	case 0:
		return sampleValue.r;
		break;
	case 1:
		return sampleValue.g;
		break;
	case 2:
		return sampleValue.b;
		break;
	case 3:
		return sampleValue.a;
		break;
	}
}

uniform int wangSeed;
#include "random.h"

vec3 HenyeyGreensteinSampleSphere ( const vec3 n, const float g ) {
	float t = ( 0.5f - g *0.5) / ( 0.5f - g + 0.5f * g * NormalizedRandomFloat() );
	float cosTheta = ( 1.0f + g * g - t ) / ( 2.0f * g );
	float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
	float phi = 2.0f * 3.14159f * NormalizedRandomFloat();

	vec3 xyz = vec3( cos( phi ) * sinTheta, sin( phi ) * sinTheta, cosTheta );
	vec3 W = ( abs(n.x) > 0.99f ) ? vec3( 0.0f, 1.0f, 0.0f ) : vec3( 1.0f, 0.0f, 0.0f );
	vec3 N = n;
	vec3 T = normalize( cross( N, W ) );
	vec3 B = cross( T, N );
	return normalize( xyz.x * T + xyz.y * B + xyz.z * N );
}

vec3 SimpleRayScatter ( const vec3 n ) {
	return normalize( n + 0.1f * RandomUnitVector() );
}

void main () {
	// invocation index
	const ivec3 myLoc = ivec3( gl_GlobalInvocationID.xyz );
	const ivec3 size = imageSize( redAtomic );
	const uvec4 blueU = blueNoiseRead( myLoc.xy );
	const vec4 blue = blueU / 255.0f;

	// init rng
	seed = wangSeed + myLoc.x * 168 + myLoc.y * 451 + blueU.x * 69 + blueU.y * 420;

	const vec3 colorScalars = vec3( NormalizedRandomFloat(), NormalizedRandomFloat(), NormalizedRandomFloat() );

	// generate ray source point
	const vec2 hexOffset = UniformSampleHexagon( blue.xy );
	const vec3 startingPoint = ( vec3( hexOffset.x, 0.0f, hexOffset.y ) * 25.0f ) + ( vec3( size ) / 10.0f );
	vec3 Direction = vec3( 1.0f, 1.0f, 0.0f );

	// do the traversal
	vec3 deltaDist = 1.0f / abs( Direction );
	ivec3 rayStep = ivec3( sign( Direction ) );
	bvec3 mask0 = bvec3( false );
	ivec3 mapPos0 = ivec3( floor( startingPoint + 0.0f ) );
	vec3 sideDist0 = ( sign( Direction ) * ( vec3( mapPos0 ) - startingPoint ) + ( sign( Direction ) * 0.5f ) + 0.5f ) * deltaDist;

	#define MAX_RAY_STEPS 2200
	for ( int i = 0; i < MAX_RAY_STEPS && ( all( greaterThanEqual( mapPos0, ivec3( 0 ) ) ) && all( lessThan( mapPos0, size ) ) ); i++ ) {
		// Core of https://www.shadertoy.com/view/4dX3zl Branchless Voxel Raycasting
		bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
		vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
		ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

		// write some test values
		imageAtomicAdd( redAtomic, mapPos0, uint( colorScalars.x * 100 ) );
		imageAtomicAdd( greenAtomic, mapPos0, uint( colorScalars.y * 22 ) );
		imageAtomicAdd( blueAtomic, mapPos0, uint( colorScalars.z * 69 ) );
		imageAtomicAdd( countAtomic, mapPos0, 100 );

		// evaluate chance to scatter, recompute traversal vars
		if ( NormalizedRandomFloat() < 0.001f ) {
			// multiply throughput by the albedo, if scattering occurs
			Direction = SimpleRayScatter( Direction );
			// Direction = HenyeyGreensteinSampleSphere( Direction, 0.76f );
			deltaDist = 1.0f / abs( Direction );
			rayStep = ivec3( sign( Direction ) );
			mask0 = bvec3( false );
			// mapPos0 = mapPos0;
			// sideDist0 = ( sign( Direction ) * ( vec3( mapPos0 ) - vec3( mapPos0 ) ) + ( sign( Direction ) * 0.5f ) + 0.5f ) * deltaDist;
			sideDist0 = ( ( sign( Direction ) * 0.5f ) + 0.5f ) * deltaDist;
		}

	// load from the idx buffer to see if we need to check against a primitive
		// uvec2 read = imageLoad( idxBuffer, mapPos0 ).xy;
		// if ( read.r & 0xFFFF000 != 0.0f ) {
			// this should be the hit condition
		// }

		sideDist0 = sideDist1;
		mapPos0 = mapPos1;
	}

	// imageStore( colorBuffer, ivec3( startingPoint ), vec4( 1.0f, 0.1f, 0.1f, 1.0f ) );

}

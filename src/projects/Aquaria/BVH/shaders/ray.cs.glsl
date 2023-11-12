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

void main () {
	// invocation index
	const ivec3 myLoc = ivec3( gl_GlobalInvocationID.xyz );
	const uvec4 blueU = blueNoiseRead( myLoc.xy );
	const vec4 blue = blueU / 255.0f;

	// init rng
	seed = wangSeed + myLoc.x * 168 + myLoc.y * 451 + blueU.x * 69 + blueU.y * 420;

	// generate ray source point
	const vec2 hexOffset = UniformSampleHexagon( blue.xy );
	const vec3 startingPoint = ( vec3( hexOffset.x, 0.0f, hexOffset.y ) * 10.0f ) + ( vec3( imageSize( redAtomic ) ) / 2.0f );
	// const vec3 startingPoint = vec3( RandomInUnitDisk(), 0.0f ).xzy * 10.0f + ( vec3( imageSize( redAtomic ) ) / 2.0f );
	// const vec3 startingPoint = RandomUnitVector() * 50.0f + ( imageSize( redAtomic ) / 2.0f );
	// const vec3 startingPoint =imageSize( redAtomic ) / 2.0f;
	const vec3 direction = vec3( 0.0f, 1.0f, 0.0f );

	// do the traversal

		// write some test values
		imageAtomicAdd( redAtomic, ivec3( startingPoint ), 100 );
		imageAtomicAdd( greenAtomic, ivec3( startingPoint ), 22 );
		imageAtomicAdd( blueAtomic, ivec3( startingPoint ), 69 );
		imageAtomicAdd( countAtomic, ivec3( startingPoint ), 100 );

	// imageStore( colorBuffer, ivec3( startingPoint ), vec4( 1.0f, 0.1f, 0.1f, 1.0f ) );

}

#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 2, r32ui ) uniform uimage2D ifsAccumulator;

layout( binding = 0, std430 ) buffer maxBuffer { uint maxCount; };

#include "mathUtils.h"
#include "random.h"
#include "complexNumbers.glsl.h"
uniform int wangSeed;

uniform float scale;
uniform vec2 offset;
uniform float rotation;

ivec2 map3DPointTo2D( vec3 p ) {
	const ivec2 is = ivec2( imageSize( ifsAccumulator ).xy );
	const float ratio = is.x / is.y;
	// want to probably pass in the trident matrix, here
	p = Rotate3D( rotation, vec3( 0.0f, 0.0f, 1.0f ) ) * p;
	p = p * scale;
	p.xy += offset;
	const ivec2 xyPos = ivec2(
		RangeRemapValue( p.x, -ratio, ratio, 0.0f, float( is.x ) ),
		RangeRemapValue( p.y, -1.0f, 1.0f, 0.0f, float( is.y ) ) );
	return xyPos;
}

float rand() { return 2.0f * ( NormalizedRandomFloat() - 0.5f ); }
void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	seed = wangSeed + writeLoc.x * 42069 + writeLoc.y * 69420;

	// vec3 p = Rotate3D( 0.5f, vec3( 1.0f ) ) * vec3( NormalizedRandomFloat() - 0.5f, NormalizedRandomFloat() - 0.5f, NormalizedRandomFloat() - 0.5f );

	// float val = 5.0f * ( rand() );
	// vec3 p = vec3( 0.1f * val, cos( val ), tan( val ) );
	// p.xy += CircleOffset() * 0.1f * p.z;

	// vec3 p = vec3( rand(), rand(), rand() * 10.0f );
	// vec3 p = vec3( CircleOffset(), rand() * 10.0f );
	vec3 p = vec3( UniformSampleHexagon(), rand() * 10.0f );
	for ( int i = 0; i < 30; i++ ) {
		p.xy = cx_mobius( p.xy );
		p.xy = cx_z_squared_plus_c( p.xy, vec2( 0.2f, 0.1f * p.z ) );
		p.xy = cx_z_plus_one_over_z( p.xy );
		ivec2 location = map3DPointTo2D( p );
		atomicMax( maxCount, imageAtomicAdd( ifsAccumulator, location, 1 ) + 1 );
	}


	// write the data to the image
}

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

	vec3 p = Rotate3D( 0.5f, vec3( 1.0f ) ) * vec3( 3.0f * RingBokeh(), rand() );

	// vec3 p = vec3( rand(), rand(), rand() * 10.0f );
	// vec3 p = vec3( CircleOffset(), rand() * 10.0f );
	// vec3 p = vec3( UniformSampleHexagon(), rand() * 10.0f );

	for ( int i = 0; i < 30; i++ ) {
		int pick = int( floor( 8.0f * NormalizedRandomFloat() ) );
		switch ( pick ) {
			case 0:
				p.xy = cx_mobius( p.xy );
			break;

			case 1:
				p.xy = cx_sin_of_one_over_z( p.zy );
			break;

			case 2:
				p.xy = cx_z_plus_one_over_z( p.yz );
			break;

			case 3:
				p.xy = cx_to_polar( p.xy );
			break;

			case 4:
				p.xy = cx_log( p.xy );
			break;

			case 5:
				p.xy = cx_tan( p.xy );
			break;

			case 6:
				p.xy = cx_tan( p.zy );
			break;

			case 7:
				p.xy = cx_tan( p.zx );
			break;

			default:
			break;
		}

		// dof offset
		// p.xy += 0.01f * UniformSampleHexagon() * ( p.z + 0.5f );

		// write the data to the image
		ivec2 location = map3DPointTo2D( p );
		atomicMax( maxCount, imageAtomicAdd( ifsAccumulator, location, 1 ) + 1 );
	}
}

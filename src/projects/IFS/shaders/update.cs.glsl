#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 2, r32ui ) uniform uimage2D ifsAccumulatorR;
layout( binding = 3, r32ui ) uniform uimage2D ifsAccumulatorG;
layout( binding = 4, r32ui ) uniform uimage2D ifsAccumulatorB;

layout( binding = 0, std430 ) buffer maxBuffer { uint maxCount[ 3 ]; };

#include "mathUtils.h"
#include "random.h"
#include "complexNumbers.glsl.h"
uniform int wangSeed;

uniform mat3 tridentMatrix;
uniform float scale;
uniform vec2 offset;

ivec2 map3DPointTo2D( vec3 p ) {
	const ivec2 is = ivec2( imageSize( ifsAccumulatorR ).xy );
	const float ratio = is.x / is.y;
	const mat3 invTridentMatrix = inverse( tridentMatrix );

	// still need to figure out the particulars of the order of operations here
	p = p * scale;
	// p.xy += ( invTridentMatrix * vec3( offset, 0.0f ) ).xy;
	p = tridentMatrix * p;
	return ivec2(
		RangeRemapValue( p.x + offset.x * scale, -ratio, ratio, 0.0f, float( is.x ) ),
		RangeRemapValue( p.y + offset.y * scale, -1.0f, 1.0f, 0.0f, float( is.y ) ) );
}

float rand() { return 2.0f * ( NormalizedRandomFloat() - 0.5f ); }
void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	seed = wangSeed + writeLoc.x * 42069 + writeLoc.y * 69420;

	vec3 p = vec3( rand(), rand(), rand() );
	vec3 color = vec3( 1.0f );

	for ( int i = 0; i < 15; i++ ) {
		int pick = int( floor( 10.0f * NormalizedRandomFloat() ) );
		// int pick = 1;
		switch ( pick ) {
			case 0:
				p.xy = cx_mobius( p.zy ) + vec2( 0.1f );
				color *= vec3( 0.9f, 0.85f, 0.9f );
			break;

			case 1:
				p.xy = cx_sin_of_one_over_z( p.zy ) + vec2( 0.2f );
				color *= vec3( 0.9f, 0.75f, 0.6f );
			break;

			case 2:
				p.xy = cx_z_plus_one_over_z( p.yz );
				color *= vec3( 0.0f, 0.4f, 0.8f );
			break;

			case 3:
				p.xy = cx_to_polar( p.yz );
				color *= vec3( 0.9f, 0.4f, 0.7f );
			break;

			case 4:
				p.xy = cx_log( p.xy );
			break;

			case 5:
				p.xy = cx_tan( p.yz );
			break;

			case 6:
				p.xy = cx_tan( p.zy );
			break;

			case 7:
				p.xy = cx_tan( p.zy );
			break;

			case 8:
				p.yz = cx_sqrt( p.xy );
			break;

			case 9:
				p.yx = cx_sqrt( p.zy );
			break;

			default:
			break;
		}

		// dof offset
		// p.xy += 0.01f * UniformSampleHexagon() * ( p.z + 0.5f );

		// write the data to the image
		ivec2 location = map3DPointTo2D( p );

		uvec3 amt = uvec3( 100.0f * color );
		atomicMax( maxCount[ 0 ], imageAtomicAdd( ifsAccumulatorR, location, amt.r ) + amt.r );
		atomicMax( maxCount[ 1 ], imageAtomicAdd( ifsAccumulatorG, location, amt.g ) + amt.g );
		atomicMax( maxCount[ 2 ], imageAtomicAdd( ifsAccumulatorB, location, amt.b ) + amt.b );
	}
}

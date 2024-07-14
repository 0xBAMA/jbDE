#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 2, r32ui ) uniform uimage2D ifsAccumulator;

layout( binding = 0, std430 ) buffer maxBuffer { uint maxCount; };

#include "mathUtils.h"
#include "random.h"
uniform int wangSeed;

ivec2 map3DPointTo2D( vec3 p ) {
	const ivec2 is = ivec2( imageSize( ifsAccumulator ).xy );
	const float ratio = is.x / is.y;
	p = p / 3.0f;
	const ivec2 xyPos = ivec2(
		RangeRemapValue( p.x, -ratio, ratio, 0.0f, float( is.x ) ),
		RangeRemapValue( p.y, -1.0f, 1.0f, 0.0f, float( is.y ) ) );
	return xyPos;
}

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	seed = wangSeed + writeLoc.x * 42069 + writeLoc.y * 69420;

	vec3 p = Rotate3D( 0.5f, vec3( 1.0f ) ) * vec3( NormalizedRandomFloat() - 0.5f, NormalizedRandomFloat() - 0.5f, NormalizedRandomFloat() - 0.5f );

	ivec2 location = map3DPointTo2D( p );

	// write the data to the image
	atomicMax( maxCount, imageAtomicAdd( ifsAccumulator, location, 1 ) + 1 );
}

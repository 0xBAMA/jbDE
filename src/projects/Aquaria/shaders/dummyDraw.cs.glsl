#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

layout( binding = 2, rg32ui ) uniform uimage3D idxBuffer;
layout( binding = 3, rgba16f ) uniform image3D dataCacheBuffer;

uniform float time;
uniform vec2 resolution;
uniform float scale;
uniform mat3 invBasis;
uniform vec3 blockSize;

#define PI 3.1415f

#include "intersect.h"

float RemapRange ( const float value, const float iMin, const float iMax, const float oMin, const float oMax ) {
	return ( oMin + ( ( oMax - oMin ) / ( iMax - iMin ) ) * ( value - iMin ) );
}

float blueNoiseRead ( ivec2 loc, int idx ) {
	ivec2 wrappedLoc = loc % imageSize( blueNoiseTexture );
	uvec4 sampleValue = imageLoad( blueNoiseTexture, wrappedLoc );
	switch ( idx ) {
	case 0:
		return sampleValue.r / 255.0f;
		break;
	case 1:
		return sampleValue.g / 255.0f;
		break;
	case 2:
		return sampleValue.b / 255.0f;
		break;
	case 3:
		return sampleValue.a / 255.0f;
		break;
	}
}


void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec3 col = vec3( 0.0f );

	// remapped uv
	vec2 subpixelOffset = vec2(
		blueNoiseRead( ivec2( gl_GlobalInvocationID.xy ), 1 ),
		blueNoiseRead( ivec2( gl_GlobalInvocationID.xy ), 2 )
	);
	vec2 uv = ( vec2( gl_GlobalInvocationID.xy ) + subpixelOffset ) / resolution.xy;
	uv = ( uv - 0.5f ) * 2.0f;

	// aspect ratio correction
	uv.x *= ( resolution.x / resolution.y );

	// box intersection
	float tMin, tMax;
	vec3 Origin = invBasis * vec3( scale * uv, -2.0f );
	vec3 Direction = invBasis * normalize( vec3( uv * 0.1f, 2.0f ) );

	// then intersect with the AABB
	const bool hit = Intersect( Origin, Direction, -blockSize / 2.0f, blockSize / 2.0f, tMin, tMax );

	if ( hit ) { // texture sample
		// for trimming edges
		const float epsilon = -0.003f; 
		const vec3 hitpointMin = Origin + tMin * Direction;
		const vec3 hitpointMax = Origin + tMax * Direction;
		const vec3 blockUVMin = vec3(
			RemapRange( hitpointMin.x, -blockSize.x / 2.0f, blockSize.x / 2.0f, 0.0f - epsilon, 1.0f + epsilon ),
			RemapRange( hitpointMin.y, -blockSize.y / 2.0f, blockSize.y / 2.0f, 0.0f - epsilon, 1.0f + epsilon ),
			RemapRange( hitpointMin.z, -blockSize.z / 2.0f, blockSize.z / 2.0f, 0.0f - epsilon, 1.0f + epsilon )
		);

		const vec3 blockUVMax = vec3(
			RemapRange( hitpointMax.x, -blockSize.x / 2.0f, blockSize.x / 2.0f, 0.0f - epsilon, 1.0f + epsilon ),
			RemapRange( hitpointMax.y, -blockSize.y / 2.0f, blockSize.y / 2.0f, 0.0f - epsilon, 1.0f + epsilon ),
			RemapRange( hitpointMax.z, -blockSize.z / 2.0f, blockSize.z / 2.0f, 0.0f - epsilon, 1.0f + epsilon )
		);

		const float thickness = abs( tMin - tMax );
		col = vec3( thickness );
	}

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}

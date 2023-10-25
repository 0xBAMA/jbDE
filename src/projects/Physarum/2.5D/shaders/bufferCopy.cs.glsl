#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform usampler2D continuum;
uniform vec2 resolution;
uniform vec3 color;
uniform vec3 blockSize;
uniform float brightness;
uniform float scale;
uniform mat3 invBasis;

#include "intersect.h"

float RemapRange ( const float value, const float iMin, const float iMax, const float oMin, const float oMax ) {
	return ( oMin + ( ( oMax - oMin ) / ( iMax - iMin ) ) * ( value - iMin ) );
}

void main () {
	// remapped uv
	vec2 uv = ( vec2( gl_GlobalInvocationID.xy ) + vec2( 0.5f ) ) / resolution.xy;
	uv = ( uv - 0.5f ) * 2.0f;

	// aspect ratio correction
	uv.x *= ( resolution.x / resolution.y );

	// box intersection
	float tMin, tMax;
	vec3 Origin = invBasis * vec3( scale * uv, -1.0f );
	vec3 Direction = invBasis * vec3( 0.0f, 0.0f, 1.0f );
	const bool hit = Intersect( Origin, Direction, -blockSize / 2.0f, blockSize / 2.0f, tMin, tMax );

	if ( hit ) {
		// texture sample
		const vec3 hitpointMin = Origin + tMin * Direction;
		const vec3 hitpointMax = Origin + tMax * Direction;
		const vec3 blockUVMin = vec3(
			RemapRange( hitpointMin.x, -blockSize.x / 2.0f, blockSize.x / 2.0f, 0.0f, 1.0f ),
			RemapRange( hitpointMin.y, -blockSize.y / 2.0f, blockSize.y / 2.0f, 0.0f, 1.0f ),
			RemapRange( hitpointMin.z, -blockSize.z / 2.0f, blockSize.z / 2.0f, 0.0f, 1.0f )
		);

		float result = texture( continuum, blockUVMin.xy ).r / 1000000.0f;
		const float thickness = abs( tMin - tMax );
		const vec3 final = brightness * result * color + vec3( 0.01f );

		imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( final, 1.0f ) );
	} else {
		imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( 0.0, 0.0f, 0.0f, 1.0f ) );
	}
}

#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform usampler2D continuum;
uniform vec2 resolution;
uniform vec3 color;
uniform float brightness;

#include "intersect.h"

void main () {
	// remapped uv
	vec2 uv = ( vec2( gl_GlobalInvocationID.xy ) + vec2( 0.5f ) ) / resolution.xy;
	uv = ( uv - 0.5f ) * 2.0f;

	// aspect ratio correction
	uv.x *= ( resolution.x / resolution.y );

	// scaling
	// uv *= scale;

	// box intersection
	float tMin, tMax;
	vec3 Origin = vec3( uv, -1.0f );
	vec3 Direction = vec3( 0.0f, 0.0f, 1.0f );
	const bool hit = Intersect( Origin, Direction, vec3( -0.1f, -0.2f, -0.3f ), vec3( 0.1f, 0.2f, 0.3f ), tMin, tMax );

	imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( hit ? uv : vec2( 0.0f ), 0.0f, 1.0f ) );
}

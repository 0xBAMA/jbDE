#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// texture for the atomic writes
layout( r32ui ) uniform uimage2D vectorscopeImage;

// texture for the composited output
layout( rgba8 ) uniform image2D compositedResult;

// buffer for max pixel count
layout( binding = 5, std430 ) buffer perColumnMins { uint maxCount; };

// rgb <-> oklch
#include "oklabConversions.h.glsl"

void main () {
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	const vec2 normalizedSpace = ( vec2( loc ) + vec2( 0.5f ) ) / imageSize( compositedResult ) - vec2( 0.5f );

	vec4 color = vec4( 0.0f, 0.0f, 0.0f, 1.0f );
	const float radius = sqrt( dot( normalizedSpace, normalizedSpace ) );
	if ( radius < 0.5f ) {
		// todo: gridlines, background
		// smoothstep for edge falloff

		// find the pixel count value, normalize by dividing by maxCount
		const float value = float( imageLoad( vectorscopeImage, loc ).r ) / float( maxCount );

		// find the color for this pixel, from position -> lch color, set l=value -> rgb
		color.rgb = oklch_to_srgb( vec3( value, radius, atan( normalizedSpace.x, normalizedSpace.y ) ) );
	}

	imageStore( compositedResult, loc, color );
}
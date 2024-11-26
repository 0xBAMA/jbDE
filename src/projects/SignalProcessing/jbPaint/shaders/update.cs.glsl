#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 2, rgba16f ) uniform image2D paintBuffer;

uniform ivec2 noiseOffset;
vec4 Blue( in ivec2 loc ) { return imageLoad( blueNoiseTexture, ( loc + noiseOffset ) % imageSize( blueNoiseTexture ).xy ); }

#include "biasGain.h"
#include "pbrConstants.glsl"
#include "mathUtils.h"

uniform vec3 brushColor;
uniform float brushRadius;
uniform float brushSlope;
uniform float brushThreshold;

// xy position + click state in z
uniform ivec3 mouseState;

void main () {
	// pixel location
	ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// pixel distance to the mouse
	const float d = distance( vec2( loc ), vec2( mouseState.xy ) );

	// draw if close to the mouse and the mouse is clicking
	if ( ( d < brushRadius ) && ( mouseState.z != 0 ) ) {
		vec4 previousValue = imageLoad( paintBuffer, loc );
		float falloff = 1.0f - biasGain( d / brushRadius, brushSlope, brushThreshold );
		float noise = RangeRemapValue( Blue( loc ).x, 0.0f, 255.0f, 0.618f, 1.0f );

		imageStore( paintBuffer, loc, previousValue + vec4( falloff * brushColor * noise, 1.0f ) );
	}
}

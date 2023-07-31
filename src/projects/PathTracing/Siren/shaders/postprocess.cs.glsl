#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

uniform sampler2D source;
layout( rgba8 ) uniform image2D displayTexture;

#include "tonemap.glsl" // tonemapping curves

uniform int tonemapMode;
uniform float gamma;
uniform vec3 colorTempAdjust;
uniform vec2 resolution;

void main () {
	ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	vec2 sampleLoc = ( vec2( loc ) + vec2( 0.5f ) ) / resolution;
	sampleLoc.y = 1.0f - sampleLoc.y;
	vec4 originalValue = texture( source, sampleLoc );
	
	vec3 color = tonemap( tonemapMode, colorTempAdjust * vec3( originalValue.rgb ) );
	color = gammaCorrect( gamma, color );
	uvec4 tonemappedValue = uvec4( uvec3( color * 255.0f ), originalValue.a * 255.0f );

	imageStore( displayTexture, loc, originalValue );
}

#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

uniform sampler2D sourceC;
uniform sampler2D sourceDN;
layout( rgba8 ) uniform image2D displayTexture;

#include "tonemap.glsl" // tonemapping curves

uniform int tonemapMode;
uniform float gamma;
uniform mat3 saturation;
uniform vec3 colorTempAdjust;
uniform vec2 resolution;

void main () {
	ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	vec2 sampleLoc = ( vec2( loc ) + vec2( 0.5f ) ) / resolution;
	vec4 originalValue = texture( sourceC, sampleLoc );
	vec4 originalDepth = texture( sourceDN, sampleLoc );

	originalValue.rgb = saturation * originalValue.rgb;
	originalValue.rgb = colorTempAdjust * originalValue.rgb;
	originalValue.rgb = Tonemap( tonemapMode, originalValue.rgb );
	originalValue.rgb = GammaCorrect( gamma, originalValue.rgb );

	imageStore( displayTexture, loc, originalValue );
	// imageStore( displayTexture, loc, originalDepth );
}

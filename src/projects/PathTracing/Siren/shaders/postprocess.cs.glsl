#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

uniform sampler2D sourceC;
uniform sampler2D sourceDN;
layout( rgba8 ) uniform image2D displayTexture;

#include "tonemap.glsl" // tonemapping curves

uniform int tonemapMode;
uniform float gamma;
uniform float saturation;
uniform vec3 colorTempAdjust;
uniform vec2 resolution;

vec3 ApplySaturation ( in vec3 colorIn ) {
	// https://www.graficaobscura.com/matrix/index.html
	const float s = saturation;
	const float oms = 1.0f - s;

	// vec3 weights = vec3( 0.2990f, 0.5870f, 0.1140f ); // NTSC weights
	vec3 weights = vec3( 0.3086f, 0.6094f, 0.0820f ); // "improved" luminance vector

	return mat3(
		oms * weights.r + s,	oms * weights.r,		oms * weights.r,
		oms * weights.g,		oms * weights.g + s,	oms * weights.g,
		oms * weights.b,		oms * weights.b,		oms * weights.b + s
	) * colorIn;
}

void main () {
	ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	vec2 sampleLoc = ( vec2( loc ) + vec2( 0.5f ) ) / resolution;
	sampleLoc.y = 1.0f - sampleLoc.y;
	vec4 originalValue = texture( sourceC, sampleLoc );

	originalValue.rgb = ApplySaturation( originalValue.rgb );
	originalValue.rgb = colorTempAdjust * originalValue.rgb;
	originalValue.rgb = Tonemap( tonemapMode, originalValue.rgb );
	originalValue.rgb = GammaCorrect( gamma, originalValue.rgb );

	imageStore( displayTexture, loc, originalValue );
}

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
uniform vec3 bgColor;

bool inBounds ( in ivec2 loc ) {
	return !(
		loc.x <= 0 ||
		loc.y <= 0 ||
		loc.x >= imageSize( displayTexture ).x - 1 ||
		loc.y >= imageSize( displayTexture ).y - 1 );

	// vec2 fLoc = loc / imageSize( displayTexture );
	// return !( fLoc.x < 0.0f || fLoc.y < 0.0f || fLoc.x > 1.0f || fLoc.y > 1.0f );
}

#define OUTPUT		0
#define ACCUMULATOR	1
#define NORMAL		2
#define DEPTH		3

uniform int activeMode;

void main () {

	// there is something wrong with the sampling here - I'm not sure exactly what it is
		// I need to put an XOR or something with exact per-pixel detail in sourceC and evaluate

	ivec2 loc = ivec2( gl_GlobalInvocationID.xy ) - ivec2( 1 );
	vec2 sampleLoc = ( vec2( loc ) + vec2( 0.5f ) ) / resolution;
	vec4 originalValue;

	switch ( activeMode ) {
		case OUTPUT:
			originalValue = texture( sourceC, sampleLoc );
			originalValue.rgb = saturation * originalValue.rgb;
			originalValue.rgb = colorTempAdjust * originalValue.rgb;
			originalValue.rgb = Tonemap( tonemapMode, originalValue.rgb );
			originalValue.rgb = GammaCorrect( gamma, originalValue.rgb );
			break;

		case ACCUMULATOR:
			originalValue = texture( sourceC, sampleLoc );
			break;

		case NORMAL:
			originalValue = texture( sourceDN, sampleLoc );
			break;

		case DEPTH:
			originalValue.rgb = vec3( 5.0f / texture( sourceDN, sampleLoc ).a );
			break;

		default:
			originalValue.rgb = vec3( 0.0f );
			break;
	}

	originalValue.a = 1.0f;

	if ( inBounds( loc ) ) {
		imageStore( displayTexture, loc, originalValue );
	} else {
		imageStore( displayTexture, loc, vec4( bgColor, 1.0f ) );
	}
}

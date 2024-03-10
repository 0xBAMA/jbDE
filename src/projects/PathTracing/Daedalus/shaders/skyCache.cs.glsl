#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( rgba32f ) uniform image2D skyCache;
layout( rgba8 ) uniform image2D blueNoise; // todo: dithering

uniform float skyTime;
#include "sky.h"

uniform int mode;
uniform vec3 color1;
uniform vec3 color2;

// https://www.shadertoy.com/view/ttcyRS
vec3 oklab_mix( vec3 colA, vec3 colB, float h ) {
	// https://bottosson.github.io/posts/oklab
	const mat3 kCONEtoLMS = mat3(
		0.4121656120f,  0.2118591070f,  0.0883097947f,
		0.5362752080f,  0.6807189584f,  0.2818474174f,
		0.0514575653f,  0.1074065790f,  0.6302613616f );
	const mat3 kLMStoCONE = mat3(
		4.0767245293f, -1.2681437731f, -0.0041119885f,
		-3.3072168827f, 2.6093323231f, -0.7034763098f,
		0.2307590544f, -0.3411344290f,  1.7068625689f );
	vec3 lmsA = pow( kCONEtoLMS * colA, vec3( 1.0f / 3.0f ) );
	vec3 lmsB = pow( kCONEtoLMS * colB, vec3( 1.0f / 3.0f ) );
	vec3 lms = mix( lmsA, lmsB, h );
	// gain in the middle (no oaklab anymore, but looks better?) -iq
	// lms *= 1.0f + 0.2f * h * ( 1.0f - h );
	return kLMStoCONE * ( lms * lms * lms );
}

void main () {
	vec3 col;

	switch ( mode ) {
		case 0: col = color1;
		break;

		case 1: col = oklab_mix( color1, color2, ( float( gl_GlobalInvocationID.y ) + 0.5f ) / float( imageSize( skyCache ).y ) );
		break;

		case 2: col = skyColor();
		break;
	}

	// write the data to the image
	imageStore( skyCache, ivec2( gl_GlobalInvocationID.xy ), vec4( col, 1.0f ) );
}

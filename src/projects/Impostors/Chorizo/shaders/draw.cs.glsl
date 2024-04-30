#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

#define UINT_MAX (0xFFFFFFFF-1)

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// rasterizer result
uniform sampler2D depthTex;
uniform sampler2D depthTexBack;
uniform sampler2D normals;
uniform sampler2D normalsBack;
uniform usampler2D primitiveID;
uniform usampler2D primitiveIDBack;

// rng seeding
#include "random.h"
uniform int inSeed;

// SSAO Constants / Support Functions
// uniform int AONumSamples;
// uniform float AOIntensity;
// uniform float AOScale;
// uniform float AOBias;
// uniform float AOSampleRadius;
// uniform float AOMaxDistance;
const int AONumSamples = 16;
const float AOIntensity = 3.0f;
const float AOScale = 1.0f;
const float AOBias = 0.05f;
const float AOSampleRadius = 0.02f;
const float AOMaxDistance = 0.07f;

// tbd which are actually needed
// uniform mat4 projTransform;
// uniform mat4 projTransformInverse;
// uniform mat4 viewTransform;
// uniform mat4 viewTransformInverse;
// uniform mat4 combinedTransform;
uniform mat4 combinedTransformInverse;
vec3 posFromTexcoord ( vec2 uv ) { // get worldspace position, by way of the depth texture
	const float depthSample = texture( depthTex, uv ).r * 2.0f - 1.0f;
	const vec4 clipPos = vec4( uv * 2.0f - 1.0f, depthSample, 1.0f );
	// vec4 viewPos = projTransformInverse * clipPos;
	// viewPos /= viewPos.w;
	// return vec4( viewTransformInverse * viewPos ).xyz;
	const vec4 viewPos = combinedTransformInverse * clipPos;
	return viewPos.xyz / viewPos.w;
}

float DoAO ( const vec2 texCoord, const vec2 uv, const vec3 p, const vec3 cNormal ) {
	vec3 diff = posFromTexcoord( texCoord + uv ) - p;
	const float l = length( diff );
	const vec3 v = diff / l;
	const float d = l * AOScale;
	float AO = max( 0.0f, dot( cNormal, v ) - AOBias ) * ( 1.0f / ( 1.0f + d ) );
	AO *= smoothstep( AOMaxDistance, AOMaxDistance * 0.5f, l );
	return AO;
}

// adapted from https://www.shadertoy.com/view/Ms33WB by Reinder Nijhoff 2016
float SpiralAO ( vec2 uv, vec3 p, vec3 n, float rad ) {
	float goldenAngle = 2.4f;
	float ao = 0.0f;
	float inv = 1.0f / float( AONumSamples );
	float radius = 0.0f;

	float rotatePhase = NormalizedRandomFloat() * 6.28f;
	float rStep = inv * rad;
	vec2 spiralUV = vec2( 0.0f );

	for ( int i = 0; i < AONumSamples; i++ ) {
		spiralUV.x = sin( rotatePhase );
		spiralUV.y = cos( rotatePhase );
		radius += rStep;
		ao += DoAO( uv, spiralUV * radius, p, n );
		rotatePhase += goldenAngle;
	}
	ao *= inv;
	return ao;
}

void main () {
	// pixel location + rng seeding
	const ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	const vec2 screenPos = vec2( writeLoc + 0.5f ) / textureSize( depthTex, 0 ).xy;
	seed = inSeed + 10612 * writeLoc.x + 385609 * writeLoc.y;

	// data from the framebuffer
	const uint idSample = texelFetch( primitiveID, writeLoc, 0 ).r;
	const vec3 normalSample = texelFetch( normals, writeLoc, 0 ).rgb;

	// solved position
	const vec3 worldPos = posFromTexcoord( screenPos );

	if ( idSample != 0 ) { // these are texels which wrote out a fragment during the raster geo pass

		float AOFactor = 1.0f - SpiralAO( screenPos, worldPos, normalSample, AOSampleRadius / texture( depthTex, screenPos ).r ) * AOIntensity;

		// write the data to the image
		// imageStore( accumulatorTexture, writeLoc, vec4( normalSample, 1.0f ) );
		imageStore( accumulatorTexture, writeLoc, vec4( vec3( AOFactor ), 1.0f ) );

	} else {
		// else this is a background colored pixel
		imageStore( accumulatorTexture, writeLoc, vec4( 0.0f ) );
	}
}

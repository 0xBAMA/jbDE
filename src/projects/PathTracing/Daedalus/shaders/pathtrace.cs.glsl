#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( location = 0, rgba8ui ) uniform uimage2D blueNoise;
layout( location = 1, rgba32f ) uniform image2D accumulatorColor;
layout( location = 2, rgba32f ) uniform image2D accumulatorNormalsAndDepth;

uniform ivec2 tileOffset;
uniform ivec2 noiseOffset;
uniform int wangSeed;
#include "random.h"		// rng + remapping utilities
#include "twigl.glsl"	// noise, some basic math utils
#include "hg_sdf.glsl"	// SDF modeling + booleans, etc

bool BoundsCheck( in ivec2 loc ) {
	const ivec2 b = ivec2( imageSize( accumulatorColor ) ).xy;
	return ( loc.x < b.x && loc.y < b.y && loc.x >= 0 && loc.y >= 0 );
}

vec4 Blue( in ivec2 loc ) {
	return imageLoad( blueNoise, ( loc + noiseOffset ) % imageSize( blueNoise ).xy );
}

struct result_t {
	vec4 color;
	vec4 normalD;
};

result_t ColorSample( in vec2 uv ) {

	// get the camera ray for this uv

	// initialize the "previous", "current" values

	// initialize state for the ray ( finalColor, throughput )

	// for bounces

		// get the scene intersection

		// update the previous ray values

		// epsilon bump

		// precalculate the reflected, diffuse, specular vectors

		// if the ray escapes

			// contribution from the skybox

		// else

			// material specific behavior

		// russian roulette termination

	// return final color * exposure - depth included


	// placeholder contents
	vec4 blue = vec4( Blue( ivec2( gl_GlobalInvocationID.xy ) + tileOffset.xy ).xyz / 255.0f, 1.0f );
	return result_t( blue, vec4( 0.0f ) );
}

void ProcessTileInvocation( in ivec2 pixel ) {

	// calculate the UV from the pixel location
	vec2 uv = ( vec2( pixel ) + vec2( 0.5f ) ) / imageSize( accumulatorColor ).xy;

	// get the new color sample
	result_t newData = ColorSample( uv );

	// load the previous color, normal, depth values
	vec4 previousColor = imageLoad( accumulatorColor, pixel );
	vec4 previousNormalD = imageLoad( accumulatorNormalsAndDepth, pixel );

	// increment the sample count
	float sampleCount = previousColor.a + 1.0f;

	// mix the new and old values, write back
	// blended with history based on sampleCount
	const float mixFactor = 1.0f / sampleCount;
	const vec4 mixedColor = vec4( mix( previousColor.rgb, newData.color.rgb, mixFactor ), sampleCount );
	const vec4 mixedNormalD = vec4(
		clamp( mix( previousNormalD.xyz, newData.normalD.xyz, mixFactor ), -1.0f, 1.0f ), // normals need to be -1..1
		mix( previousNormalD.w, newData.normalD.w, mixFactor )
	);

	// store the values back
	imageStore( accumulatorColor, pixel, mixedColor );
	imageStore( accumulatorNormalsAndDepth, pixel, mixedNormalD );

	// I'm thinking that it maybe actually makes more sense to do tonemapping, etc
	// here - I will have the mixed result, ready - I can write that to the 
	// accumulatorColor, tonemap the value, and store to the tonemapped texture ?
		// this might be later, I'm not doing it for the initial impl
}

void main() {
	const ivec2 location = ivec2( gl_GlobalInvocationID.xy ) + tileOffset.xy;
	if ( BoundsCheck( location ) ) {
		// seed defined in random.h, value used for the wang hash
		seed = wangSeed + 42069 * location.x + 451 * location.y;
		ProcessTileInvocation( location );
	}
}
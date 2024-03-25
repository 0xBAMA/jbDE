#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( location = 0, rgba8ui ) uniform uimage2D blueNoise;
layout( location = 1, rgba32f ) uniform image2D accumulatorColor;
layout( location = 2, rgba32f ) uniform image2D accumulatorNormalsAndDepth;
layout( location = 3, rgba32f ) uniform image2D tonemappedResult;

uniform int tonemapMode;
uniform float gamma;
uniform float postExposure;
uniform mat3 saturation;
uniform vec3 colorTempAdjust;

// vignetting
uniform bool enableVignette;
uniform float vignettePower;

#include "tonemap.glsl"

void main() { // This is where tonemapping etc will be happening on the accumulated image
	const ivec2 location = ivec2( gl_GlobalInvocationID.xy );
	vec4 color = imageLoad( accumulatorColor, location );

	// lens distort - tbd, texture binding, also?

	// chromatic aberration

	// SSAO, since we have access to depth and normals

	// potentially somet kind of denoising, in the future

	// ...

	// exposure adjustment
	color.rgb *= postExposure;

	// vignetting
	if ( enableVignette ) {
		vec2 uv = ( location + vec2( 0.5f ) ) / vec2( imageSize( tonemappedResult ).xy );
		uv *= 1.0f - uv.yx;
		color.rgb *= pow( uv.x * uv.y, vignettePower );
	}

	// tonemapping
	color.rgb = Tonemap( tonemapMode, colorTempAdjust * ( saturation * color.rgb ) );

	// gamma
	color.rgb = GammaCorrect( gamma, color.rgb );

	// dithering

	imageStore( tonemappedResult, location, color );
}
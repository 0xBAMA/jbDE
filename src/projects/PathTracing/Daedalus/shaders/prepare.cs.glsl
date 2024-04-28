#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( location = 0, rgba8ui ) uniform uimage2D blueNoise;
layout( location = 1, rgba32f ) uniform image2D accumulatorColor;
layout( location = 2, rgba32f ) uniform image2D accumulatorNormalsAndDepth;
layout( location = 3, rgba32f ) uniform image2D tonemappedResult;

uniform sampler2D accumulatorColorTex;

// general tonemapping/grading parameters
uniform int tonemapMode;
uniform float gamma;
uniform float postExposure;
uniform mat3 saturation;
uniform vec3 colorTempAdjust;

// lens distortion
uniform bool enableLensDistort;
uniform int lensDistortNumSamples;
// parameters to the Brown-Conrady distort ( k1, k2, skew )
uniform vec3 lensDistortParametersStart;
uniform vec3 lensDistortParametersEnd;
uniform int lensDistortColorWeightMode;

// vignetting
uniform bool enableVignette;
uniform float vignettePower;
uniform float hueShift;

#include "tonemap.glsl"

void main() { // This is where tonemapping etc will be happening on the accumulated image
	const ivec2 location = ivec2( gl_GlobalInvocationID.xy );
	vec4 color = vec4( 0.0f );
	if ( lensDistortEnable == true ) {
		for ( int i = 0; i < lensDistortNumSamples; i++ ) {
			float interpolationValue = ( i + 0.5f ) / float( lensDistortNumSamples );
			vec3 interpolatedParameters = mix( lensDistortParametersStart, lensDistortParametersEnd, interpolationValue );

			vec3 colorWeight = vec3( 1.0f );
			switch ( lensDistortColorWeightMode ) {
				case 1: // linear version
					// if ( interpolationValue < 0.5f ) { // red to green if less than midpoint, green to blue if greater
					// 	weight[ blue ]	= RangeRemapValue( interpolationValue, 0.0f, midPoint, 1.0f, 0.0f );
					// 	weight[ green ]	= ( 1.0f - weight[ blue ] ) / 2.0f;
					// 	weight[ red ]	= 0.0f;
					// } else {
					// 	weight[ red ]	= RangeRemapValue( interpolationValue, midPoint, iterations, 0.0f, 1.0f );
					// 	weight[ blue ]	= 0.0f;
					// 	weight[ green ]	= ( 1.0f - weight[ red ] ) / 2.0f;
					// }
					break;
				case 2: // smooth version
					colorWeight = vec3( sin( interpolationValue ), sin( interpolationValue * 2.0f ), cos( interpolationValue ) );
					break;
				default: break;
			}

			// sample the colors out at points along the interpolated Brown-Conrady distort
			// color = imageLoad( accumulatorColor, location );

		}
	} else {
		color = imageLoad( accumulatorColor, location );
	}

	// hue shifting
	color.rgb = oklch_to_srgb( srgb_to_oklch( color.rgb ) + vec3( 0.0f, 0.0f, hueShift ) );

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
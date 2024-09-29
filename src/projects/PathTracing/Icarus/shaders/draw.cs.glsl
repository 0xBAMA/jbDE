#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform sampler2D outputImage;

uniform float time;

// #include "spaceFillingCurves.h.glsl"
// #include "colorRamps.glsl.h"
#include "mathUtils.h"

// // distance to line segment
// float sdSegment2 ( vec2 p, vec2 a, vec2 b ) {
// 	vec2 pa = p - a, ba = b - a;
// 	float h = clamp( dot( pa, ba ) / dot( ba, ba ), 0.0f, 1.0f );
// 	return length( pa - ba * h );
// }

uniform float scale;
uniform vec2 offset;

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	vec3 col = vec3( 0.0f );

	// we got the click and drag much nicer this time
	const vec2 is = vec2( imageSize( accumulatorTexture ).xy );
	vec2 offsetLoc = vec2( writeLoc ) + vec2( 0.5f );
	vec2 normalizedPosition =  2.0f * offsetLoc / is.xy - vec2( 1.0f );
	const float aspectRatio = is.x / is.y;
	normalizedPosition.x *= aspectRatio;
	vec2 uv = scale * ( normalizedPosition ) + offset;

	// debug checkerboard
	// if ( step( 0.0f, cos( pi * uv.x + pi / 2.0f ) * cos( pi * uv.y + pi / 2.0f ) ) == 0 ) {
		// col = vec3( 1.0f );
	// }

	const vec2 ts = textureSize( outputImage, 0 );
	vec2 texUV = uv * 1000.0f;
	if ( texUV.x >= 0 && texUV.y >= 0 && texUV.x < ts.x && texUV.y < ts.y ) {
		// this is where we need to do the Adam sampling
		col = vec3( 1.0f );
	} else {
		// guide lines
		float verticalFalloff = exp( -0.01f * abs( ts.y - texUV.y ) );
		float xStep = smoothstep( 10.0f, 0.0f, abs( texUV.x - ts.x / 2.0f ) ) * verticalFalloff;
		float yStep = smoothstep( 10.0f, 0.0f, abs( texUV.y - ts.y / 2.0f ) );
		// col = vec3( xStep + yStep );
		col = vec3( verticalFalloff );
	}

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}

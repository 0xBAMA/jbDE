#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform float time;

// #include "spaceFillingCurves.h.glsl"
// #include "colorRamps.glsl.h"

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

	// I want consistiency in the click/drag/zoom logic this time
		// this is a priority

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}

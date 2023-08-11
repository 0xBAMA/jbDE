#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba16f ) uniform image2D source;
layout( binding = 1, rgba8ui ) uniform uimage2D displayTexture;

#include "tonemap.glsl" // tonemapping curves

uniform int tonemapMode;
uniform float gamma;
uniform mat3 saturation;
uniform vec3 colorTempAdjust;

void main () {
	ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// temporary hack for inverted image
	vec4 originalValue = imageLoad( source, ivec2( loc.x, imageSize( source ).y - loc.y - 1 ) );

	vec3 color = Tonemap( tonemapMode, colorTempAdjust * ( saturation * originalValue.rgb ) );
	color = GammaCorrect( gamma, color );
	uvec4 tonemappedValue = uvec4( uvec3( color * 255.0f ), originalValue.a * 255.0f );

	imageStore( displayTexture, loc, tonemappedValue );
}

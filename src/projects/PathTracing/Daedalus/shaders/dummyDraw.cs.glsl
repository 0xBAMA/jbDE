#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform float scale;
uniform vec2 offset;

// https://www.shadertoy.com/view/wdK3Dy
float Grid( vec2 fragCoord, float space, float gridWidth ) {
	vec2 p  = fragCoord - vec2( 0.5f );
	vec2 size = vec2( gridWidth );
	vec2 a1 = mod( p - size, space );
	vec2 a2 = mod( p + size, space );
	vec2 a = a2 - a1;
	float g = min( a.x, a.y );
	return clamp( g, 0.0f, 1.0f );
}

// todo: try bgolus's thing
// https://www.shadertoy.com/view/mdVfWw

float BGolusPristineGrid( in vec2 uv, in vec2 ddx, in vec2 ddy, vec2 lineWidth ) {
	vec2 uvDeriv = vec2( length( vec2( ddx.x, ddy.x ) ), length( vec2( ddx.y, ddy.y ) ) );
	bvec2 invertLine = bvec2( lineWidth.x > 0.5f, lineWidth.y > 0.5f );
	vec2 targetWidth = vec2(
		invertLine.x ? 1.0f - lineWidth.x : lineWidth.x,
		invertLine.y ? 1.0f - lineWidth.y : lineWidth.y );
	vec2 drawWidth = clamp( targetWidth, uvDeriv, vec2( 0.5f ) );
	vec2 lineAA = uvDeriv * 1.5f;
	vec2 gridUV = abs( fract( uv ) * 2.0f - 1.0f );
	gridUV.x = invertLine.x ? gridUV.x : 1.0f - gridUV.x;
	gridUV.y = invertLine.y ? gridUV.y : 1.0f - gridUV.y;
	vec2 grid2 = smoothstep(drawWidth + lineAA, drawWidth - lineAA, gridUV);

	grid2 *= clamp( targetWidth / drawWidth, 0.0f, 1.0f );
	grid2 = mix( grid2, targetWidth, clamp( uvDeriv * 2.0f - 1.0f, 0.0f, 1.0f ) );
	grid2.x = invertLine.x ? 1.0f - grid2.x : grid2.x;
	grid2.y = invertLine.y ? 1.0f - grid2.y : grid2.y;
	return mix( grid2.x, 1.0f, grid2.y );
}

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec2 pixelLocation = vec2( writeLoc ) * scale + offset;
	vec2 pixelLocation_neighbor = vec2( writeLoc + ivec2( 1, 0 ) ) * scale + offset;
	vec3 ditherValue = imageLoad( blueNoiseTexture, writeLoc % imageSize( blueNoiseTexture ) ).rgb * 0.0005f;
	bool zoomedIn = ( scale < 0.4f );

	vec2 deriv = vec2( 0.0f, abs( pixelLocation.x - pixelLocation_neighbor.x ) );
	vec3 result = vec3( zoomedIn ? BGolusPristineGrid( pixelLocation, deriv.yx, deriv.xy, vec2( 0.1f ) ) * 0.4f: 0.1f );

	// could jack in a little drop shadow, without much difficulty...

		// I would also like the grid lines to get dimmer, for high zoom levels - not fully disappear, but certainly subtler

	if ( abs( pixelLocation.x ) < 640 && abs( pixelLocation.y ) < 480 ) {
		result += vec3( 1.0f, 0.0f, 0.0f );
		if ( !zoomedIn ) {
			result = vec3( 1.0f, 0.0f, 0.0f );
		}
	} else {
		vec2 values = smoothstep( vec2( 1.25f ), vec2( 0.0f ), abs( pixelLocation ) );
		result += vec3( values.x + values.y ) * 0.0618f;
		result -= ditherValue;
	}

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( result, 1.0f ) );
}

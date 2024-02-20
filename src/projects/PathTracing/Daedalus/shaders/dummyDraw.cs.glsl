#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform float scale;
uniform vec2 offset;

void AddGridlines( inout vec3 colorResult, in vec3 lineColor, in vec2 gridSizing, in vec2 pixelPosition ) {
	// vec2 result = step( fract( pixelPosition / gridSizing ) * gridSizing, vec2( 1.0f ) );
	vec2 result = smoothstep( vec2( 1.0f ), vec2( 0.0f ), fract( pixelPosition / gridSizing ) * gridSizing );
	if ( max( result.x, result.y ) > 0.0f ) {
		colorResult += lineColor;
	}
}

vec3 GridBGSample( in vec2 position ) {
	vec3 accum = vec3( 0.0f );
	AddGridlines( accum, vec3( 0.1618f ), vec2( 16.0f ), position );
	AddGridlines( accum, vec3( 0.1618f, 0.0f, 0.0f ), vec2( 80.0f ), position );
	return accum;
}

void main () {
	// pixel location
	// ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy ) + offset;
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	// drawing gridlines - I want to do something with this https://www.shadertoy.com/view/MdlGRr - will make it simple to do scaled grids
	vec3 accum = vec3( 0.0f );
	// if ( writeLoc.x % 10 == 0  || writeLoc.y % 10 == 0 )	accum += vec3( 0.1f );
	// if ( writeLoc.x % 100 == 0 || writeLoc.y % 100 == 0 )	accum.r += 0.1f;

	const float num = 4;
	for ( float x = 0; x < num; x++ ) {
		for ( float y = 0; y < num; y++ ) {
			accum += GridBGSample( ( vec2( writeLoc ) + vec2( x, y ) / 2.0f + vec2( 0.25f ) ) * scale ) / ( num * num );
		}
	}

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( vec3( accum ), 1.0f ) );
}

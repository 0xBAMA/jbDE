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

void main () {
	// pixel location
	// ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy ) + offset;
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	vec3 accum = vec3( 0.0f );
	const float num = 4;
	for ( float x = 0; x < num; x++ ) {
		for ( float y = 0; y < num; y++ ) {
			vec2 location = scale * ( vec2( writeLoc ) + vec2( x, y ) / num );
			vec3 temp = vec3( 0.5f );
			temp *=
				Grid( location, 10.0f, 0.5f ) *
				Grid( location, 50.0f, 1.0f );
			accum += temp / ( num * num );
		}
	}

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( vec3( accum ), 1.0f ) );
}

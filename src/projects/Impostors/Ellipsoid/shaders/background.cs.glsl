#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

float RemapRange( const float value,
	const float iMin, const float iMax,
	const float oMin, const float oMax ) {
	return ( oMin + ( ( oMax - oMin ) /
		( iMax - iMin ) ) * ( value - iMin ) );
}

vec2 RemapRange( const vec2 value,
	const vec2 iMin, const vec2 iMax,
	const vec2 oMin, const vec2 oMax ) {
	return vec2(
		RemapRange( value.x, iMin.x, iMax.x, oMin.x, oMax.x ),
		RemapRange( value.y, iMin.y, iMax.y, oMin.y, oMax.y )
	);
}

void main() {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	const vec2 fractionalUV =
		( vec2( writeLoc ) + vec2( 0.5f ) ) /
		vec2( imageSize( accumulatorTexture ).xy );

	const vec2 centeredUV = RemapRange( fractionalUV,
		vec2( 0.0f ), vec2( 1.0f ),
		vec2( -1.0f, -1.0f ), vec2( 1.0f, 1.0f ) );

	vec2 uv = fractionalUV;
	uv *= 1.0f - uv.yx;
	float vignetteFactor = pow( uv.x * uv.y * 1.0f, 0.5f );

	vec3 col = imageLoad( blueNoiseTexture, ivec2( vignetteFactor * writeLoc ) % imageSize( blueNoiseTexture ) ).bbb / 255.0f;

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( col, 1.0f ) );
}

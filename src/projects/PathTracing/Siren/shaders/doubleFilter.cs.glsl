#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( rgba32f ) uniform image2D sourceData;
layout( rgba32f ) uniform image2D destData;

// median filter code from Nameless
float luma ( vec3 c ) {
	return 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
}

vec3 pickbetween3( vec3 a, vec3 b, vec3 c ) {
	float lm1 = luma( a );
	float lm2 = luma( b );
	float lm3 = luma( c );

	float lowest = min( lm1, min( lm2, lm3 ) );
	float maxest = max( lm1, max( lm2, lm3 ) );

	if ( lowest == lm1 ) {
		if ( maxest == lm2 ) {
			return c;
		} else {
			return b;
		}
	}

	if ( lowest == lm2 ) {
		if ( maxest == lm1 ) {
			return c;
		} else {
			return a;
		}
	}

	if ( lowest == lm3 ) {
		if ( maxest == lm1 ) {
			return b;
		} else {
			return a;
		}
	}

	return a;
}

vec3 median ( ivec2 uv ) {
	const vec3 c = vec3( 0.0f );
	vec3 colArray[9] = vec3[](c,c,c,c,c,c,c,c,c);

	for ( int i = 0; i < 9; i++ ) {
		ivec2 offset = ivec2( ( i % 3 ) - 1, ( i / 3 ) - 1 );
		vec3 currCol = imageLoad( sourceData, uv + offset ).xyz;
		colArray[ i ] = currCol;
	}

	vec3 first =	pickbetween3( colArray[ 0 ], colArray[ 1 ], colArray[ 2 ] );
	vec3 second =	pickbetween3( colArray[ 3 ], colArray[ 4 ], colArray[ 5 ] );
	vec3 third =	pickbetween3( colArray[ 6 ], colArray[ 7 ], colArray[ 8 ] );
	return pickbetween3( first, second, third );
}

void main () {
	// write the data to the image
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	const float sampleCount = imageLoad( sourceData, writeLoc ).a;
	vec3 kernelResult = ( // 3x3 gaussian kernel
		1.0f * median( writeLoc + ivec2( -1, -1 ) ) +
		1.0f * median( writeLoc + ivec2( -1,  1 ) ) +
		1.0f * median( writeLoc + ivec2(  1, -1 ) ) +
		1.0f * median( writeLoc + ivec2(  1,  1 ) ) +
		2.0f * median( writeLoc + ivec2(  0,  1 ) ) +
		2.0f * median( writeLoc + ivec2(  0, -1 ) ) +
		2.0f * median( writeLoc + ivec2(  1,  0 ) ) +
		2.0f * median( writeLoc + ivec2( -1,  0 ) ) +
		4.0f * median( writeLoc + ivec2(  0,  0 ) ) ) / 16.0f;
	imageStore( destData, writeLoc, vec4( kernelResult, sampleCount ) );
}

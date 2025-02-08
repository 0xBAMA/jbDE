#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 4, rg32ui ) uniform uimage2D ruleBuffer;

// uniform ivec2 hashgridOffset;

uvec2 getRule () {
	uint rule[ 25 ] = {
		0, 1, 0, 2, 2,
		0, 2, 2, 1, 0,
		2, 0, 0, 2, 1,
		0, 0, 0, 2, 0,
		2, 0, 0, 2, 2
	};

	uvec2 encodedRule = uvec2( 0 );
	for ( int i = 0; i < 16; i++ ) { // encode 0..15
		encodedRule.x += ( rule[ i ] << ( 30 - 2 * i ) );
	}

	for ( int i = 16; i < 25; i++ ) { // encode 16..25
		encodedRule.y += ( rule[ i ] << ( 30 - 2 * ( i - 16 ) ) );
	}

	return encodedRule;
}

void main () {
	// figuring out what values to write... start with a constant
	imageStore( ruleBuffer, ivec2( gl_GlobalInvocationID.xy ), getRule().rgrg );
}
#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( location = 0, rgba32f ) uniform image2D histogram;

layout( binding = 1, std430 ) buffer colorHistogramR {
	uint valuesR[ 256 ];
	uint maxValueR;
};
layout( binding = 2, std430 ) buffer colorHistogramG {
	uint valuesG[ 256 ];
	uint maxValueG;
};
layout( binding = 3, std430 ) buffer colorHistogramB {
	uint valuesB[ 256 ];
	uint maxValueB;
};
layout( binding = 4, std430 ) buffer colorHistogramL {
	uint valuesL[ 256 ];
	uint maxValueL;
};

uint vmax ( uvec4 v ) {
	return max( max( v.x, v.y ), max( v.z, v.w ) );
}

void main () {
	ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	vec4 color = vec4( 0.0f, 0.0f, 0.0f, 1.0f );

	uint yBin = loc.y / 16;
	float yFract = ( 16 - ( loc.y % 16 ) ) / 16.0f;

	// uint maxxx = vmax( uvec4( maxValueR, maxValueG, maxValueB, maxValueL ) );

	const float power = 0.3f;

	// partition y into 4
	switch ( yBin ) {
		case 0:	// red
		if ( yFract < pow( valuesR[ loc.x ] / float( maxValueR ), power ) ) {
			color.r = 1.0f;
		}
		break;

		case 1: // green
		if ( yFract < pow( valuesG[ loc.x ] / float( maxValueG ), power ) ) {
			color.g = 1.0f;
		}
		break;

		case 2: // blue
		if ( yFract < pow( valuesB[ loc.x ] / float( maxValueB ), power ) ) {
			color.b = 1.0f;
		}
		break;

		case 3: // luma
		if ( yFract < pow( valuesL[ loc.x ] / float( maxValueL ), power ) ) {
			color.rgb = vec3( 1.0f );
		}
		break;
	}

	// x is 0..255
	// normalization factors are in maxValue

	imageStore( histogram, loc, color );
}
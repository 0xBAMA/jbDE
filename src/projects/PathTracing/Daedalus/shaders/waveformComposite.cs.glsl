#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// four images for atomic writes - red, green, blue, luma
layout( r32ui ) uniform uimage2D redImage;
layout( r32ui ) uniform uimage2D greenImage;
layout( r32ui ) uniform uimage2D blueImage;
layout( r32ui ) uniform uimage2D lumaImage;

// buffers for per-column min/max - r, g, b, luma interleaved
layout( binding = 2, std430 ) readonly buffer perColumnMins { uint columnMins[]; };
layout( binding = 3, std430 ) readonly buffer perColumnMaxs { uint columnMaxs[]; };
layout( binding = 4, std430 ) buffer binMax { uint globalMax[ 4 ]; };

// the output image
layout( rgba8 ) uniform image2D compositedResult;

void main () {
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	const bool drawMinMax = true;

	vec4 color = vec4( 0.0f, 0.0f, 0.0f, 1.0f );
	// gridlines

	const uint yIndex = 255 - ( loc.y % 256 );
	switch ( loc.y / 256 ) {
		case 0: // red
		if ( drawMinMax && ( yIndex == columnMins[ 4 * loc.x ] || yIndex == columnMaxs[ 4 * loc.x ] ) ) {
			color.rg = vec2( 1.0f );
		} else {
			color.r = float( imageLoad( redImage, ivec2( loc.x, yIndex ) ).r ) / float( globalMax[ 0 ] );
		}
		break;

		case 1: // green
		if ( drawMinMax && ( yIndex == columnMins[ 4 * loc.x + 1 ] || yIndex == columnMaxs[ 4 * loc.x + 1 ] ) ) {
			color.rg = vec2( 1.0f );
		} else {
			color.g = float( imageLoad( greenImage, ivec2( loc.x, yIndex ) ).r ) / float( globalMax[ 1 ] );
		}
		break;

		case 2: // blue
		if ( drawMinMax && ( yIndex == columnMins[ 4 * loc.x + 2 ] || yIndex == columnMaxs[ 4 * loc.x + 2 ] ) ) {
			color.rg = vec2( 1.0f );
		} else {
			color.b = float( imageLoad( blueImage, ivec2( loc.x, yIndex ) ).r ) / float( globalMax[ 2 ] );
		}
		break;

		case 3: // luma
		if ( drawMinMax && ( yIndex == columnMins[ 4 * loc.x + 3 ] || yIndex == columnMaxs[ 4 * loc.x + 3 ] ) ) {
			color.rg = vec2( 1.0f );
		} else {
			color.rgb = vec3( float( imageLoad( lumaImage, ivec2( loc.x, yIndex ) ).r ) / float( globalMax[ 3 ] ) );
		}
		break;
	}

	imageStore( compositedResult, loc, color );
}
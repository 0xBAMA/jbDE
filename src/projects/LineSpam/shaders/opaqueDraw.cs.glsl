#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

// depth buffer, for clearing
layout( binding = 0, r32ui ) uniform uimage2D depthBuffer;

// id buffer
layout( binding = 1, r32ui ) uniform uimage2D idBuffer;

#include "mathUtils.h"

uniform float depthRange;
uniform mat4 transform;
uniform uint numOpaque;

// opaque line buffer
struct line_t {
	vec4 p0;
	vec4 p1;
	vec4 color;
};

layout( binding = 0, std430 ) buffer opaqueLineData {
	line_t opaqueData[];
};

// parameters for the lines
ivec2 p0 = ivec2( 0 ), p1 = ivec2( 0 );
uint d0 = 0, d1 = 0;

void setPixelColor ( int x, int y, float AAFactor ) {
	uint id = gl_GlobalInvocationID.x + 4096 * gl_GlobalInvocationID.y;
	// we need to get the depth
	uint lerpedDepth = uint( mix( float( d0 ), float( d1 ), ( ( RangeRemapValue( x, p0.x, p1.x, 0.0f, 1.0f ) ) + ( RangeRemapValue( y, p0.x, p1.x, 0.0f, 1.0f ) ) ) / 2.0f ) );

	// imageAtomicMax( depthBuffer, ivec2( x, y ), lerpedDepth );
	if ( imageLoad( depthBuffer, ivec2( x, y ) ).r == lerpedDepth ) { // depth-equals check
		imageStore( idBuffer, ivec2( x, y ), uvec4( id + 1 ) );
	}
}

// line drawing - from https://zingl.github.io/bresenham.html
void plotLineWidth( int x0, int y0, int x1, int y1, float wd ) {
	int dx = abs( x1 - x0 ), sx = x0 < x1 ? 1 : -1;
	int dy = abs( y1 - y0 ), sy = y0 < y1 ? 1 : -1;
	int err = dx - dy, e2, x2, y2;	/* error value e_xy */
	float ed = dx + dy == 0 ? 1 : sqrt( float( dx ) * float( dx ) + float( dy ) * float( dy ) );

	for ( wd = ( wd + 1 ) / 2; ; ) { /* pixel loop */
		setPixelColor( x0, y0, max( 0, 255 - 255 * ( abs( err - dx + dy ) / ed - wd + 1 ) ) );
		e2 = err; x2 = x0;
		if ( 2 * e2 >= -dx ) { /* x step */
			for ( e2 += dy, y2 = y0; e2 < ed * wd && ( y1 != y2 || dx > dy ); e2 += dx )
				setPixelColor( x0, y2 += sy, max( 0, 255 - 255 * ( abs( e2 ) / ed - wd + 1 ) ) );
			if ( x0 == x1 ) break;
			e2 = err; err -= dy; x0 += sx;
		}
		if ( 2 * e2 <= dy ) { /* y step */
			for ( e2 = dx - e2; e2 < ed * wd && ( x1 != x2 || dx < dy ); e2 += dy )
				setPixelColor( x2 += sx, y0, max( 0, 255 - 255 * ( abs( e2 ) / ed - wd + 1 ) ) );
			if ( y0 == y1 ) break;
			err += dx; y0 += sy;
		}
	}
}

void main() {
	uint index = gl_GlobalInvocationID.x + 4096 * gl_GlobalInvocationID.y;

	if ( index < numOpaque ) {
		// writing id values for pixels which compute the same depth value stored in the depth buffer after pre-z
			// setting id to invocation id + 1, so we have 0 as a reserve value
		line_t line = opaqueData[ index ];

		// translate NDC to screen coords
		ivec2 iS = imageSize( depthBuffer );
		float ratio = float( iS.x ) / float( iS.y );

		// apply any desired transforms to p0 and p1
		line.p0.xyz = ( transform * vec4( line.p0.xyz, 1.0f ) ).xyz;
		line.p1.xyz = ( transform * vec4( line.p1.xyz, 1.0f ) ).xyz;

		p0 = ivec2( int( RangeRemapValue( line.p0.x, -ratio, ratio, 0.0f, iS.x ) ), int( RangeRemapValue( line.p0.y, -1.0f, 1.0f, 0.0f, iS.y ) ) );
		d0 = uint( RangeRemapValue( line.p0.z, -depthRange, depthRange, float( UINT_MAX ), 0.0f ) ); // depths
		p1 = ivec2( int( RangeRemapValue( line.p1.x, -ratio, ratio, 0.0f, iS.x ) ), int( RangeRemapValue( line.p1.y, -1.0f, 1.0f, 0.0f, iS.y ) ) );
		d1 = uint( RangeRemapValue( line.p1.z, -depthRange, depthRange, float( UINT_MAX ), 0.0f ) );

		plotLineWidth( p0.x, p0.y, p1.x, p1.y, 2.5f );
	}
}
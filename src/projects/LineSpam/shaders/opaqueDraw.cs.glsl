#version 430
layout( local_size_x = 256, local_size_y = 1, local_size_z = 1 ) in;

// depth buffer

// id buffer

// opaque line buffer
struct line_t {
	vec4 p0;
	vec4 p1;
	vec4 color;
};

layout( binding = 0, std430 ) buffer opaqueLineData {
	line_t opaqueData[];
};

// // line drawing - from https://zingl.github.io/bresenham.html
// void plotLineWidth( int x0, int y0, int x1, int y1, float wd ) {
// 	int dx = abs( x1 - x0 ), sx = x0 < x1 ? 1 : -1;
// 	int dy = abs( y1 - y0 ), sy = y0 < y1 ? 1 : -1;
// 	int err = dx - dy, e2, x2, y2;	/* error value e_xy */
// 	float ed = dx + dy == 0 ? 1 : sqrt( float( dx ) * float( dx ) + float( dy ) * float( dy ) );

// 	for ( wd = ( wd + 1 ) / 2; ; ) { /* pixel loop */
// 		// setPixelColor( x0, y0, max( 0, 255 * ( abs( err - dx + dy ) / ed - wd + 1 ) ) );
// 		e2 = err; x2 = x0;
// 		if ( 2 * e2 >= -dx ) { /* x step */
// 			for ( e2 += dy, y2 = y0; e2 < ed * wd && ( y1 != y2 || dx > dy ); e2 += dx )
// 				// setPixelColor( x0, y2 += sy, max( 0, 255 * ( abs( e2 ) / ed - wd + 1 ) ) );
// 			if ( x0 == x1 ) break;
// 			e2 = err; err -= dy; x0 += sx;
// 		}
// 		if ( 2 * e2 <= dy ) { /* y step */
// 			for ( e2 = dx - e2; e2 < ed * wd && ( x1 != x2 || dx < dy ); e2 += dy )
// 				// setPixelColor( x2 += sx, y0, max( 0, 255 * ( abs( e2 ) / ed - wd + 1 ) ) );
// 			if ( y0 == y1 ) break;
// 			err += dx; y0 += sy;
// 		}
// 	}
// }

void main() {
	
}
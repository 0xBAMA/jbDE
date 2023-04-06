#version 430

in vec3 color;
in vec3 normal;
out vec4 glFragColor;

void main() {
	glFragColor = vec4( color, 1.0f );
	glFragColor *= dot( vec3( 1.0f ), normal );
	glFragColor.a *= 0.2f;

	//these are used to draw something along the lines of scanlines (per-pixel and dithering-style effects)
	int fcxmod2 = int( gl_FragCoord.x ) % 2;
	int fcymod3 = int( gl_FragCoord.y ) % 3;
	if( ( fcymod3 == 0 ) || ( fcxmod2 == 0 ) ) {
		discard;
	}
}
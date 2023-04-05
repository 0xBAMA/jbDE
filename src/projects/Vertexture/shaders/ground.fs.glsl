#version 430

in vec3 color;
out vec4 glFragColor;

void main() {

	glFragColor = vec4( color, 1.0f );

	// glFragColor = vec4( vec3( vSColor * 255.0f ), 1.0f );
	// glFragColor = vec4( vec3( 1.0f ) * gl_FragDepth, 1.0f );

	// //fcymod3 is used to draw something along the lines of scanlines
	// int fcxmod2 = int(gl_FragCoord.x) % 2;
	// int fcymod3 = int(gl_FragCoord.y) % 3;
	// if((fcymod3 == 0) || (fcxmod2 == 0)) { //add  '&& (fcxmod2 == 0)' for x based stuff
	// 	// discard;
	// 	gl_FragColor.r *= 1.618;
	// }
}
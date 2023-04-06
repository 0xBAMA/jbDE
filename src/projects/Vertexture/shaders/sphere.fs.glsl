#version 430

in vec3 color;
out vec4 glFragColor;

uniform sampler2D sphere;

void main() {


	vec4 tRead = texture( sphere, gl_PointCoord.xy );

	if ( tRead.x < 0.05f ) discard;

	glFragColor = vec4( tRead.xyz, 1.0f );

	gl_FragDepth = gl_FragCoord.z - 0.05f * tRead.x;



	// glFragColor = vec4( 1.0 );

	// //fcymod3 is used to draw something along the lines of scanlines
	// int fcxmod2 = int(gl_FragCoord.x) % 2;
	// int fcymod3 = int(gl_FragCoord.y) % 3;
	// if((fcymod3 == 0) || (fcxmod2 == 0)) { //add  '&& (fcxmod2 == 0)' for x based stuff
	// 	// discard;
	// 	gl_FragColor.r *= 1.618;
	// }
}
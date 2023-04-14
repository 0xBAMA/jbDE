#version 430

uniform sampler2D heightmap;
uniform sampler2D waterHeight;

uniform float time;
uniform vec3 groundColor;

in vec3 color;
in vec2 texCoord;
in vec3 position;

out vec4 glFragColor;

void main() {

	vec4 tRead = texture( heightmap, texCoord );
	vec4 tReadWater = texture( waterHeight, texCoord * 4.51f + vec2( 0.8f * time ) );
	const bool belowGround = position.z < ( tRead.r * 0.4f - 0.18f );
	const bool belowWater = position.z < 0.02f;

	if ( gl_FrontFacing ){ // backdrop behavior
		if ( belowGround ) {
			// glFragColor = vec4( 1.0f, tRead.r, 0.0f, 1.0f );
			discard;
		} else {
			// glFragColor = vec4( 0.1618f, 0.5f, 0.0f, 0.8f );
			// glFragColor = vec4( vec3( 0.0f ), 1.0f );
			discard;
		}
	} else {
		if ( belowGround ) { // ground gradient

			// float grad = clamp( 0.0f, 1.0f, position.z * tRead.r );
			float grad = clamp( 0.1f, 1.0f, position.z / 0.4f + 0.2f / 0.4f );
			glFragColor = vec4( grad, grad, grad, 1.0f );

			// glFragColor.r *= 0.7f;
			// glFragColor.g *= 0.5f;
			// glFragColor.b *= 0.4f;

			glFragColor.rgb *= groundColor;

			glFragColor.a = grad * 10.0f;

		} else if ( belowWater ) { // water color
			glFragColor = vec4( 0.1f, 0.2f, 0.4f, 0.3f );
		} else {
			discard;
		}

		//fcymod3 is used to draw something along the lines of scanlines
		int fcxmod2 = int( gl_FragCoord.x ) % 2;
		int fcymod2 = int( gl_FragCoord.y ) % 2;
		if ( ( fcymod2 == 1 ) || ( fcxmod2 == 1 ) ) {
			glFragColor.rgb /= 1.618f;
		}
	}
}
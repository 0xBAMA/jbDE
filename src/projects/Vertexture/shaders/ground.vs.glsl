#version 430

uniform float AR;
uniform sampler2D heightmap;
uniform float time;
uniform float scale;
uniform mat3 trident;
uniform vec3 groundColor;

in vec3 vPosition;

out vec3 color;
out vec3 position;
out mat3 rot;

void main () {
	vec4 tRead = texture( heightmap, vPosition.xy / ( 1.618f * 2.0f ) );
	color = vec3( tRead.r );
	color.rgb *= groundColor;

	rot = trident;

	vec3 vPosition_local = scale * rot * ( vPosition + vec3( 0.0f, 0.0f, tRead.r * 0.4f - 0.2f ) );
	position = vPosition_local;

	gl_Position = vec4( vPosition_local * vec3( 1.0f, AR, 1.0f ) * 0.4f, 1.0f );
}

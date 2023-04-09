#version 430

uniform float AR;

in vec3 vPosition;
// in vec2 vTexturePosition;

out vec3 color;
out vec2 texCoord;
out vec3 position;

uniform sampler2D heightmap;
uniform float time;
uniform mat3 trident;

void main() {

	texCoord = vPosition.xy / ( 1.618f * 2.0f );
	vec4 tRead = texture( heightmap, texCoord );

	color = vec3( tRead.r );
	color.r *= 0.7f;
	color.g *= 0.5f;
	color.b *= 0.4f;

	position = vPosition;

	vec3 vPosition_local = trident * ( vPosition + vec3( 0.0f, 0.0f, tRead.r * 0.4f - 0.2f ) );

	gl_Position = vec4( vPosition_local * vec3( 1.0f, AR, 1.0f ) * 0.4f, 1.0f );
}

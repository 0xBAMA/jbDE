#version 430

uniform float AR;

in vec4 vPosition;
in vec4 vColor;
// in vec2 vTexturePosition;

uniform sampler2D heightmap;
uniform float time;
uniform mat3 trident;

out float radius;
out vec3 color;
out vec3 position;
out float height;

out mat3 rot;
flat out int index;

void main() {

	index = gl_VertexID;

	vec4 tRead = texture( heightmap, vPosition.xy / ( 1.618f * 2.0f ) );
	height = tRead.r * 0.4f - 0.2f;

	radius = gl_PointSize = vPosition.a + height * 16.0f;

	// gl_PointSize *= 1.0f + 2.0f * clamp( 0.7f, 1.0f, 2.5f * tRead.r - 2.0f );
	// color = mix( mix( vec3( 1.0f, 1.0f, 0.0f ), vec3( 0.1f, 1.0f, 0.0f ), ( radius - 1.0f ) / 6.0f ) * 0.618f, vec3( 0.2f, 0.4f, 0.9f ), 1.0f - tRead.r );
	color = vColor.xyz;

	rot = trident;

	position = trident * ( vPosition.xyz + vec3( 0.0f, 0.0f, height ) );
	gl_Position = vec4( position * vec3( 1.0f, AR, 1.0f ) * 0.4f, 1.0f );
}

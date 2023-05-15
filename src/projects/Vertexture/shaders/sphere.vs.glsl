#version 430

uniform float AR;
uniform sampler2D heightmap;
uniform float scale;
uniform float time;
uniform mat3 trident;

in vec4 vColor;
in vec4 vPosition;

out float radius;
out vec3 color;
out vec3 position;
out float height;
out mat3 rot;
flat out int index;

void main () {
	index = gl_VertexID;
	vec4 tRead = texture( heightmap, vPosition.xy / ( 1.618f * 2.0f ) );
	height = tRead.r * 0.4f - 0.2f;

	radius = gl_PointSize = scale * vPosition.a * AR;
	color = vColor.xyz;
	rot = trident;

	position = scale * trident * ( vPosition.xyz + vec3( 0.0f, 0.0f, height ) );
	position.z += radius / 1024.0f; // precompensate for depth
	gl_Position = vec4( position * vec3( 1.0f, AR, 1.0f ), 1.0f );
}

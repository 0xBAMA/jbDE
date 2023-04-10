#version 430

uniform float AR;

uniform sampler2D heightmap;
uniform float time;
uniform mat3 trident;

struct point_t {
	vec4 position;
	vec4 color;
};

layout( binding = 3, std430 ) buffer pointData {
	point_t data[];
};

out float radius;
out vec3 color;
out vec3 position;
out float height;
flat out int index;

void main() {

	index = gl_VertexID;
	vec4 positionRead = data[ index ].position;
	vec4 colorRead = data[ index ].color;

	vec4 tRead = texture( heightmap, positionRead.xy / ( 1.618f * 2.0f ) );
	height = tRead.r * 0.4f - 0.2f;

	radius = gl_PointSize = positionRead.a;

	// gl_PointSize *= 1.0f + 2.0f * clamp( 0.7f, 1.0f, 2.5f * tRead.r - 2.0f );
	// color = mix( mix( vec3( 1.0f, 1.0f, 0.0f ), vec3( 0.1f, 1.0f, 0.0f ), ( radius - 1.0f ) / 6.0f ) * 0.618f, vec3( 0.2f, 0.4f, 0.9f ), 1.0f - tRead.r );
	color = colorRead.rgb;

	position = trident * ( positionRead.xyz + vec3( 0.0f, 0.0f, height ) );
	gl_Position = vec4( position * vec3( 1.0f, AR, 1.0f ) * 0.4f, 1.0f );
}

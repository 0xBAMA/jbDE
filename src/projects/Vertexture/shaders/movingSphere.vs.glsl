#version 430

uniform float AR;
uniform sampler2D heightmap;
uniform float time;
uniform mat3 trident;
uniform float scale;
uniform float frameWidth;

// moving point state
struct point_t {
	vec4 position;
	vec4 color;
};
layout( binding = 3, std430 ) buffer pointData {
	point_t data[];
};

out float radius;
out float roughness;
out vec3 color;
out vec3 worldPosition;
out float height;
out mat3 rot;
flat out int index;

void main () {
	index = gl_VertexID;
	vec4 positionRead = data[ index ].position;
	vec4 colorRead = data[ index ].color;

	// radius = gl_PointSize = scale * positionRead.a * AR;

	color = colorRead.rgb;

	rot = trident;

	worldPosition = positionRead.xyz;

	vec3 position = scale * trident * ( worldPosition );
	radius = gl_PointSize = ( scale * positionRead.a * AR ) / max( position.z + 1.0f, 0.01f );
	position.z += radius / frameWidth; // precompensate for depth offset in fragment shader
	gl_Position = vec4( position * vec3( 1.0f, AR, 1.0f ), max( position.z + 1.0f, 0.01f ) );

}

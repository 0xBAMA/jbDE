#version 430

uniform float AR;
uniform float scale;
uniform float time;
uniform mat3 trident;
uniform float frameWidth;
uniform mat4 perspectiveMatrix;

in vec4 vColor;
in vec4 vPosition;

out float radius;
flat out float roughness;
out vec3 color;
out vec3 worldPosition;
out float height;
flat out int index;

void main () {
	index = gl_VertexID;

	color = vColor.xyz;
	roughness = vColor.a;
	worldPosition = vPosition.xyz;

	vec3 position = scale * trident * worldPosition;
	radius = gl_PointSize = ( scale * vPosition.a * AR ) / max( position.z + 1.0f, 0.01f );
	position.z += radius / frameWidth; // precompensate for depth - this needs to be scaled with the primitive if we move to quads

	gl_Position = vec4( position * vec3( 1.0f, AR, 1.0f ), max( position.z + 1.0f, 0.01f ) );
}

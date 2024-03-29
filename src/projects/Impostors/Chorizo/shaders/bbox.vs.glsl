#version 430
#include "mathUtils.h"
#include "cubeVerts.h"

out flat uint vofiIndex;
out vec4 vofiColor;
out vec3 vofiPosition;

uniform mat4 viewTransform;
layout( binding = 0, std430 ) buffer transformsBuffer {
	mat4 transforms[];
};

void main() {
	vofiIndex = gl_VertexID / 36;
	// const vec3 position = transforms[ vofiIndex ] * CubeVert( gl_VertexID % 36 );

	vec4 position = mat4( // replace with the actual transform
		0.2f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.2f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.2f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
	) * vec4( CubeVert( gl_VertexID % 36 ), 1.0f );

	vofiColor = vec4( 1.0f ); // placeholder color
	vofiPosition = position.xyz;
	gl_Position = viewTransform * position;
}
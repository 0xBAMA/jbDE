#version 430
#include "mathUtils.h"
#include "cubeVerts.h"

out flat uint vofiIndex;
out vec4 vofiColor;
out vec3 vofiPosition;

uniform mat4 viewTransform;
struct transform_t {
	mat4 transform;
	mat4 inverseTransform;
};
layout( binding = 0, std430 ) buffer transformsBuffer {
	transform_t transforms[];
};

void main() {
	vofiIndex = gl_VertexID / 36;
	vec4 position = transforms[ vofiIndex ].transform * vec4( CubeVert( gl_VertexID % 36 ), 1.0f );

	vofiColor = vec4( 1.0f ); // placeholder color
	vofiPosition = position.xyz;
	gl_Position = viewTransform * position;
}
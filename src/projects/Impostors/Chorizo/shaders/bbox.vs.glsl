#version 430
out vec4 vofiColor;

#include "mathUtils.h"
#include "cubeVerts.h"

uniform mat4 viewTransform;
layout( binding = 0, std430 ) buffer transforms {
	mat3 transforms[];
};

void main() {
	const mat3 modelTransform = transforms[ gl_VertexID / 36 ];		// stride of 36 verts
	const vec3 position = modelTransform * CubeVert( gl_VertexID % 36 );	// within the stride

	gl_Position	= viewTransform * vec4( position, 1.0f );
	vofiColor	= vec4( 1.0f );	// placeholder color
}
#version 430

#include "mathUtils.h"
uniform mat4 transform;

out vec4 vofiColor;

void main() {
	const vec4 points[ 36 ] = {
		vec4( -1.0f,-1.0f,-1.0f, 1.0f ),
		vec4( -1.0f,-1.0f, 1.0f, 1.0f ),
		vec4( -1.0f, 1.0f, 1.0f, 1.0f ),
		vec4(  1.0f, 1.0f,-1.0f, 1.0f ),
		vec4( -1.0f,-1.0f,-1.0f, 1.0f ),
		vec4( -1.0f, 1.0f,-1.0f, 1.0f ),
		vec4(  1.0f,-1.0f, 1.0f, 1.0f ),
		vec4( -1.0f,-1.0f,-1.0f, 1.0f ),
		vec4(  1.0f,-1.0f,-1.0f, 1.0f ),
		vec4(  1.0f, 1.0f,-1.0f, 1.0f ),
		vec4(  1.0f,-1.0f,-1.0f, 1.0f ),
		vec4( -1.0f,-1.0f,-1.0f, 1.0f ),
		vec4( -1.0f,-1.0f,-1.0f, 1.0f ),
		vec4( -1.0f, 1.0f, 1.0f, 1.0f ),
		vec4( -1.0f, 1.0f,-1.0f, 1.0f ),
		vec4(  1.0f,-1.0f, 1.0f, 1.0f ),
		vec4( -1.0f,-1.0f, 1.0f, 1.0f ),
		vec4( -1.0f,-1.0f,-1.0f, 1.0f ),
		vec4( -1.0f, 1.0f, 1.0f, 1.0f ),
		vec4( -1.0f,-1.0f, 1.0f, 1.0f ),
		vec4(  1.0f,-1.0f, 1.0f, 1.0f ),
		vec4(  1.0f, 1.0f, 1.0f, 1.0f ),
		vec4(  1.0f,-1.0f,-1.0f, 1.0f ),
		vec4(  1.0f, 1.0f,-1.0f, 1.0f ),
		vec4(  1.0f,-1.0f,-1.0f, 1.0f ),
		vec4(  1.0f, 1.0f, 1.0f, 1.0f ),
		vec4(  1.0f,-1.0f, 1.0f, 1.0f ),
		vec4(  1.0f, 1.0f, 1.0f, 1.0f ),
		vec4(  1.0f, 1.0f,-1.0f, 1.0f ),
		vec4( -1.0f, 1.0f,-1.0f, 1.0f ),
		vec4(  1.0f, 1.0f, 1.0f, 1.0f ),
		vec4( -1.0f, 1.0f,-1.0f, 1.0f ),
		vec4( -1.0f, 1.0f, 1.0f, 1.0f ),
		vec4(  1.0f, 1.0f, 1.0f, 1.0f ),
		vec4( -1.0f, 1.0f, 1.0f, 1.0f ),
		vec4(  1.0f,-1.0f, 1.0f, 1.0f )
	};

	const vec4 colors[ 3 ] = {
		vec4( 1.0f, 0.0f, 0.0f, 1.0f ),
		vec4( 0.0f, 1.0f, 0.0f, 1.0f ),
		vec4( 0.0f, 0.0f, 1.0f, 1.0f )
	};

	gl_Position	= transform * points[ gl_VertexID ];
	vofiColor	= colors[ gl_VertexID % 3 ];
}
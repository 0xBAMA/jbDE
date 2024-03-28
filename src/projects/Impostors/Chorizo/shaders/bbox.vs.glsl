#version 430

#include "mathUtils.h"
uniform mat4 mvp;
uniform mat4 transform;

out vec4 vofiColor;

vec4 CubeVert( int idx ) {
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

	return points[ idx % 36 ];
}

void main() {
	const vec4 colors[ 3 ] = {
		vec4( 1.0f, 0.0f, 0.0f, 1.0f ),
		vec4( 0.0f, 1.0f, 0.0f, 1.0f ),
		vec4( 0.0f, 0.0f, 1.0f, 1.0f )
	};

	vec4 pos = transform * CubeVert( gl_VertexID );
	gl_Position	= mvp * pos;
	vofiColor	= pos;
}
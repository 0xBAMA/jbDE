#version 430

out vec4 vofiColor;

void main() {

	// hello triangle
	const vec4 points[ 3 ] = {
		vec4(  0.0f,  0.5f,  0.0f, 1.0f ),
		vec4(  0.5f, -0.5f,  0.0f, 1.0f ),
		vec4( -0.5f, -0.5f,  0.0f, 1.0f )
	};

	const vec4 colors[ 3 ] = {
		vec4(  1.0f,  0.0f,  0.0f, 1.0f ),
		vec4(  0.0f,  1.0f,  0.0f, 1.0f ),
		vec4(  0.0f,  0.0f,  1.0f, 1.0f )
	};

	gl_Position	= points[ gl_VertexID ];
	vofiColor	= colors[ gl_VertexID ];
}
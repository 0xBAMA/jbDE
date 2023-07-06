#version 430 core

layout( binding = 1, r32ui ) uniform uimage2D current;

uniform bool showAgents;

out vec4 fragmentOutput;

void main () {
	if( showAgents ) {
		float distToCenter = distance( gl_PointCoord, vec2( 0.5f, 0.5f ) );
		fragmentOutput = smoothstep( vec4( 1.0f, 0.0f, 0.0f, 1.0f ), vec4( 0.0f ), vec4( 2.0f * distToCenter ) );
	} else {
		discard;
	}
}

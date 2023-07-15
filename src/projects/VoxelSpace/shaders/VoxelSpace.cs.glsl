#version 430
layout( local_size_x = 16, local_size_y = 1, local_size_z = 1 ) in;

// layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( rgba8ui ) uniform uimage2D map;
layout( rgba16f ) uniform image2D target;

// uniform float time;

void main () {

	// imageStore( target, ivec2( gl_GlobalInvocationID.xy ), vec4( 1.0f, 0.0f, 0.0f, 0.2f ) );

}
#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform float time;

void main () {
	// pixel location
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	vec3 accum = vec3( 0.0f );
	if ( writeLoc.x % 10 == 0 || writeLoc.y % 10 == 0 ) {
		accum += vec3( 0.1f );
	}
	if ( writeLoc.x % 100 == 0 || writeLoc.y % 100 == 0 ) {
		accum.r += 0.1f;
	}

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( vec3( accum ), 1.0f ) );
}

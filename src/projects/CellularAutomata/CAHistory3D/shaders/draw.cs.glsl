#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// current state of the Cellular automata
layout( binding = 2, r8ui ) uniform uimage3D CAStateBuffer;
uniform vec2 resolution;

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec3 color = vec3( 0.0f );

	// generate eye ray
	const vec2 uv = ( ( vec2( writeLoc ) + vec2( 0.5f ) ) / vec2( imageSize( accumulatorTexture ) ) ) - vec2( 0.5f );
	color.rg = uv;

	// ray-plane test

	// dda traversal, from intersection point

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( color, 1.0f ) );
}

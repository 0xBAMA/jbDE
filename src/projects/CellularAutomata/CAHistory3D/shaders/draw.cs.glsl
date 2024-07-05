#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// current state of the Cellular automata
layout( binding = 2, r8ui ) uniform uimage3D CAStateBuffer;

uniform vec3 viewerPosition;

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec3 color = vec3( 0.0f );

	const vec2 iS = imageSize( accumulatorTexture ).xy;

	// generate eye ray
	const vec2 uv = vec2( writeLoc + 0.5f ) / iS;
	const vec2 p = ( uv * 2.0f - 1.0f ) * vec2( iS.x / iS.y, 1.0f );




	// ray-plane test

	// dda traversal, from intersection point

	// write the data to the image
	color.rg = p;
	if ( distance( p, vec2( 0.0f ) ) < 0.1f ) {
		color = vec3( 1.0f );
	}

	imageStore( accumulatorTexture, writeLoc, vec4( color, 1.0f ) );
}

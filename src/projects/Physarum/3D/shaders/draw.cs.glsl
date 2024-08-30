#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform usampler3D continuum;
uniform sampler3D shadedVolume;
uniform vec2 resolution;
uniform vec3 color;
uniform float brightness;
uniform float zOffset;

// what are we doing?

void main () {
	vec2 uv = ( ( gl_GlobalInvocationID.xy + vec2( 0.5f ) ) / resolution ) - vec2( 0.5f );
	uv.x *= ( float( imageSize( accumulatorTexture ).x ) / float( imageSize( accumulatorTexture ).y ) );

	// we will transform the block, sample in world coords making a couple assumptions
		// ray starting points are on a constant grid, and we rotate the volume if we want that



}

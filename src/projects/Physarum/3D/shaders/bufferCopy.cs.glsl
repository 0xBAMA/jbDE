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

// wanting to do some kind of visualization of the voxels
// https://www.shadertoy.com/view/NstSR8

void main () {
	const vec2 uv = ( ( gl_GlobalInvocationID.xy + vec2( 0.5f ) ) / resolution ) - vec2( 0.5f );

	imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( uv.xy, 0.0f, 1.0f ) );
}

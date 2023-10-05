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
	// const vec2 sampleLocation = ( vec2( gl_GlobalInvocationID.xy ) + vec2( 0.5f ) ) / resolution.xy;
	// uint result = texture( continuum, vec3( sampleLocation, zOffset ) ).r;
	// // write the data to the accumulator
	// imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( brightness * ( result / 1000000.0f ) * color, 1.0f ) );



	const vec2 sampleLocation = ( vec2( gl_GlobalInvocationID.xy ) + vec2( 0.5f ) ) / resolution.xy;
	// vec3 result = vec3( texture( continuum, vec3( sampleLocation, zOffset ) ).r / 1000.0f );
	vec3 result = texture( shadedVolume, vec3( sampleLocation, zOffset ) ).rgb;
	// write the data to the accumulator
	imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( result, 1.0f ) );



	// // doing the voxel visualization
	// const vec2 sampleLocation = ( vec2( gl_GlobalInvocationID.xy ) + vec2( 0.5f ) ) / resolution.xy;
	// uint result = 0;
	// for ( int i = 0; i < 64; i++ ) {
	// 	uint shadowDepth = 0;
	// 	for ( int j = 0; j < 16; j++ ) {
	// 		shadowDepth += texture( continuum, vec3( sampleLocation, ( i + 0.5f ) / 64.0f ) + vec3( 0.001f * j ) ).r;
	// 	}

	// 	// result += uint( texture( continuum, vec3( sampleLocation, ( i + 0.5f ) / 64.0f ) ).r * ( 1.0f / shadowDepth ) );
	// 	result += shadowDepth;
	// }
	// imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( brightness * ( result / 1000000.0f ) * color, 1.0f ) );

}

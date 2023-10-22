#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;

// store the result with the color + opacity included
layout( rgba16f ) uniform image3D shadedVolume;

uniform usampler3D continuum;
uniform vec3 resolution;
uniform vec3 color;
uniform float brightness;
uniform float zOffset;
uniform vec3 lightDirection;

// wanting to do some kind of visualization of the voxels
// https://www.shadertoy.com/view/NstSR8

void main () {
	// doing the voxel visualization
	const vec3 sampleLocation = ( vec3( gl_GlobalInvocationID.xyz ) + vec3( 0.5f ) ) / imageSize( shadedVolume ).xyz;
	uint myValue = texture( continuum, sampleLocation ).r;
	float densityFraction = myValue / 100.0f;

	// first light
	// float blueNoiseValue = imageLoad( blueNoiseTexture, ivec2( gl_GlobalInvocationID.xy ) ).r / 255.0f;
	uint shadowDepth = 0;
	for ( int j = 0; j < 64; j++ ) {
		// shadowDepth += texture( continuum, sampleLocation + 0.01f * j * lightDirection ).r;
		shadowDepth += texture( continuum, vec3( 0.5f ) - sampleLocation + 0.01f * j * lightDirection ).r;
	}

	// shade
	vec3 colorResult = brightness * densityFraction * color;
	colorResult *= exp( -float( shadowDepth / 400000.0f ) ) + 0.0618f;

	// imageStore( shadedVolume, ivec3( gl_GlobalInvocationID.xyz ), vec4( vec2( shadowDepth / 1000000.0f ), myValue / 10000.0f, 1.0f ) );
	imageStore( shadedVolume, ivec3( gl_GlobalInvocationID.xyz ), vec4( colorResult, myValue / 16000.0f ) );
	// imageStore( shadedVolume, ivec3( gl_GlobalInvocationID.xyz ), vec4( colorResult, 1.0f ) );
	// imageStore( shadedVolume, ivec3( gl_GlobalInvocationID.xyz ), vec4( vec3( myValue / 100000.0f ), 1.0f ) );

	// imageStore( shadedVolume, ivec3( gl_GlobalInvocationID.xyz ), vec4( vec3( gl_GlobalInvocationID.x ^ gl_GlobalInvocationID.y ^ gl_GlobalInvocationID.z ) / 255.0f, 1.0f ) );
}

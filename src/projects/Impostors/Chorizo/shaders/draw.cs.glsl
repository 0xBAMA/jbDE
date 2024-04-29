#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D normals;
layout( binding = 2, rgba16f ) uniform image2D normalsBack;
layout( binding = 3, r32ui ) uniform uimage2D primitiveID;
layout( binding = 4, r32ui ) uniform uimage2D primitiveIDBack;
layout( binding = 5, rgba16f ) uniform image2D accumulatorTexture;

uniform float time;

#define UINT_MAX (0xFFFFFFFF-1)

void main () {
	// pixel location
	const ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	// data from the framebuffer
	const uint idSample = imageLoad( primitiveID, writeLoc ).r;
	const vec3 normalSample = imageLoad( normals, writeLoc ).rgb;

	// todo: compute SSAO

	if ( idSample != UINT_MAX ) { // these are texels which wrote out a fragment during the raster geo pass
		// write the data to the image
		imageStore( accumulatorTexture, writeLoc, vec4( normalSample, 1.0f ) );

	} else {
		// else this is a background colored pixel
		imageStore( accumulatorTexture, writeLoc, vec4( 0.0f ) );
	}
}

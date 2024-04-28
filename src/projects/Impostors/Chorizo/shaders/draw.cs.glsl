#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D normals;
layout( binding = 2, rgba16f ) uniform image2D normalsBack;
layout( binding = 3, r32ui ) uniform uimage2D primitiveID;
layout( binding = 4, r32ui ) uniform uimage2D primitiveIDBack;
layout( binding = 5, rgba16f ) uniform image2D accumulatorTexture;

uniform float time;

void main () {
	// pixel location
	const ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	const uint idSample = imageLoad( primitiveID, writeLoc ).r;
	const vec3 normalSample = imageLoad( normals, writeLoc ).rgb;


	if ( idSample != 0 ) { // these are texels which wrote out a fragment during the raster geo pass
		// write the data to the image
		imageStore( accumulatorTexture, writeLoc, vec4( normalSample, 1.0f ) );
	} else {
		// vec4 blue = imageLoad( blueNoiseTexture, writeLoc % imageSize( blueNoiseTexture ) ) / 255.0f;
		// const vec4 previousValue = imageLoad( accumulatorTexture, ivec2( writeLoc + blue.xy * 3.0f ) );
		// const vec4 previousValue = imageLoad( accumulatorTexture, writeLoc );
		imageStore( accumulatorTexture, writeLoc, vec4( 0.0f ) );
	}
}

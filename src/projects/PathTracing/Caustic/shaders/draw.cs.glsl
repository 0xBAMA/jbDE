#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// the field buffer image
layout( r32ui ) uniform uimage2D bufferImage;
layout( binding = 0, std430 ) buffer maxBuffer { uint maxCount; };

#include "mathUtils.h"
#include "colorRamps.glsl.h"

void main () {
	// pixel location
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	vec3 col = vec3( 0.0f );
	if ( all( lessThan( loc, imageSize( bufferImage ) ) ) ) {
		// col = vec3( float( imageLoad( bufferImage, loc ).r ) / float( maxCount ) ) * 10.0f;
		// col = vec3( float( imageLoad( bufferImage, loc ).r ) / 20.0f );
		// col = inferno( saturate( float( imageLoad( bufferImage, loc ).r ) / 20.0f ) );
		col = PuBu_r( saturate( float( imageLoad( bufferImage, loc ).r ) / 100.0f ) );
	}

	// write the data to the image
	imageStore( accumulatorTexture, loc, vec4( col, 1.0f ) );
}

#version 430
layout( local_size_x = 8, local_size_y = 8, local_size_z = 8 ) in;

// utils
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// results of the precompute step
layout( binding = 2, rg32ui ) uniform uimage3D idxBuffer;
layout( binding = 3, rgba16f ) uniform image3D dataCacheBuffer;

// breaking up the work
uniform ivec3 offset;

// sphere data
const int numSpheres = 65535;
struct sphere_t {
	vec4 positionRadius;
	vec4 colorMaterial;
};
layout( binding = 0, std430 ) buffer sphereData {
	sphere_t spheres[];
};


// uint topFourIDs[ 4 ] = { 0 };
// float topFourDists[ 4 ] = { 0.0f };
// #define REPLACES_FIRST	0
// #define REPLACES_SECOND	1
// #define REPLACES_THIRD	2
// #define REPLACES_FOURTH	3
// #define REPLACES_NONE	4
// // returns true if it replaces
// bool CheckSphere ( uint index, float dist ) {
// 	int status = REPLACES_NONE;
// 	for ( int i = 0; i < 4; i++ ) {
// 		if ( dist >  ) {

// 			break;
// 		}
// 	}
// 	switch ( status ) {
	
// 	}
// }

#include "noise.h"
#include "wood.h"
#include "hg_sdf.glsl"

float frame ( vec3 p ) {
	vec3 pCache = p;
	ivec3 bSize = imageSize( dataCacheBuffer );
	pMirror( p.x, 0.0f );
	float cD = fCylinder( p - vec3( bSize.x / 2, 0.0f, 0.0f ), bSize.z / 2.0f, bSize.y );
	pMirror( p.y, 0.0f );
	float rD = fCylinder( p.yxz - vec3( bSize.y / 2, 0.0f, 0.0f ), bSize.z / 5.0f, bSize.x );
	p = pCache;
	pModInterval1( p.x, 100.0f, -10, 10 );
	float pD = fCylinder( p, bSize.z / 7.0f, bSize.y );

	return fOpUnionRound( fOpUnionRound( cD, rD, bSize.z / 4.0f ), pD, bSize.z / 5.0f );
}

void main () {
	// voxel location
	const ivec3 writeLoc = ivec3( gl_GlobalInvocationID.xyz ) + offset;
	const vec3 pos = vec3( writeLoc ) + vec3( 0.5f ) - ( vec3( imageSize( dataCacheBuffer ) ) / 2.0f );

	// running color
	vec4 color = vec4( 0.0f );

	// // grid
	// const bool x = ( writeLoc.x + 10 ) % 168 < 20;
	// const bool y = ( writeLoc.y + 10 ) % 128 < 16;
	// const bool z = ( writeLoc.z ) % 132 < 40;
	// if ( ( x && y ) || ( x && z ) || ( y && z ) || writeLoc.x < 20 || writeLoc.y < 20 ) {
	// 	if ( perlinfbm( pos / 500.0f, 2.0f, 10 ) < 0.0f || writeLoc.x < 20 || writeLoc.y < 20 ) {
	// 		// color = vec4( vec3( 0.618f + perlinfbm( pos / 300.0f, 2.0f, 3 ) ), 1.0f );
	// 		vec3 c = matWood( pos / 60.0f );
	// 		color = vec4( c, 1.0f + dot( c, vec3( 0.299f, 0.587f, 0.114f ) ) ); // luma to set alpha
	// 	}
	// }

	if ( frame( pos ) <= 0.0f ) {

		// wood material
		vec3 c = matWood( pos.zyx / 60.0f );
		color = vec4( c, 1.0f + dot( c, vec3( 0.299f, 0.587f, 0.114f ) ) ); // luma to set alpha

		// marble type material
		// color = vec4( abs( perlinfbm( pos / 500.0f, 2.0f, 10 ) ) );
		// color.a = ( 2.0f - color.a ) * 1.2f;

	} else {

	// iterate through the spheres
		// keep top four nearest
		// keep distance to nearest
		// keep direction away from nearest as normal

		float minDistance = 10000.0f;
		for ( int i = 0; i < numSpheres; i++ ) {
			vec4 pr = spheres[ i ].positionRadius;
			float d = distance( pos, pr.xyz ) - pr.w;
			minDistance = min( minDistance, d );
			if ( minDistance == d && d <= 0.0f ) {
			// if ( minDistance == d ) {
				// color = vec4( spheres[ i ].colorMaterial.rgb, clamp( -minDistance, 0.0f, 1.0f ) );
				color = spheres[ i ].colorMaterial;


				// weird ugly thresholded voronoi thing
				// vec3 c = matWood( pos.yzx / 40.0f );
				// color.rgb = c;
				// if ( spheres[ i ].colorMaterial.a > 0.5f )
				// 	if ( d <=  0.0f ) {
				// 		color = spheres[ i ].colorMaterial;
				// 	} else {
				// 		color.a = 0.0f;
				// 	}
				// else
				// 	color.a = 1.0f + pow( dot( c, vec3( 0.299f, 0.587f, 0.114f ) ), 3.0f ) * 2.0f;


				// // partially colored with curl noise... kinda sucks
				// if ( i % 2 == 0 ){
				// 	color = vec4( curlNoise( pos / 3000.0f ) * 0.66f, spheres[ i ].colorMaterial.a );
				// } else {
				// 	color = spheres[ i ].colorMaterial;
				// }

				// ... consider checking spheres in random order?
				// break; // I speculate that there is actually only ever one sphere affecting a voxel, this is a good optimization for now
					// eventually I will want to disable this, when I'm collecting the nearest N spheres
			}
		}
	}

	// write the data to the image - pack 16-bit ids into 32 bit storage
	// imageStore( idxBuffer, writeLoc, uvec4( <closest> <second>, <third> <fourth>, 0.0f, 1.0f ) );
	imageStore( dataCacheBuffer, writeLoc, color );
}

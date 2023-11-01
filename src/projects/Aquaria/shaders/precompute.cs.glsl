#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// utils
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// results of the precompute step
layout( binding = 2, rg32ui ) uniform uimage3D idxBuffer;
layout( binding = 3, rgba16f ) uniform image3D dataCacheBuffer;

// breaking up the work
uniform int slice;

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

void main () {
	// voxel location
	const ivec3 writeLoc = ivec3( gl_GlobalInvocationID.xy, slice );
	const vec3 pos = vec3( writeLoc ) + vec3( 0.5f ) - ( vec3( imageSize( dataCacheBuffer ) ) / 2.0f ) + vec3( 0.0f, 0.0f, 64.0f );

	// iterate through the spheres
		// keep top four nearest
		// keep distance to nearest
		// keep direction away from nearest

	float minDistance = 10000.0f;
	vec4 color = vec4( 0.0f );
	for ( int i = 0; i < numSpheres; i++ ) {
		vec4 pr = spheres[ i ].positionRadius;
		float d = distance( pos, pr.xyz ) - pr.w;
		minDistance = min( minDistance, d );
		if ( minDistance == d && d <= 0.0f ) {
			color = vec4( spheres[ i ].colorMaterial.rgb, 1.0f );
		}
		// if ( distance( spheres[ i ].positionRadius.xyz, pos ) < spheres[ i ].positionRadius.w ) {
			// color = spheres[ i ].colorMaterial.rgb;
		// }
	}

	// if ( distance( pos, vec3( 0.0f ) ) < 100.0f ) {
		/// affirmative
		// imageStore( dataCacheBuffer, writeLoc, vec4( vec3( ( writeLoc.x ^ writeLoc.y ^ writeLoc.z ) / 255.0f ), 1.0f ) );
	// }

	// write the data to the image - pack 16-bit ids into 32 bit storage
	// imageStore( idxBuffer, writeLoc, uvec4( <closest> <second>, <third> <fourth>, 0.0f, 1.0f ) );
	// imageStore( dataCacheBuffer, writeLoc, vec4( vec3( minDistance ), 1.0f ) );
	// imageStore( dataCacheBuffer, writeLoc, vec4( vec3( ( minDistance < 0.0f ) ? 1.0f : 0.0f ), 1.0f ) );
	// imageStore( dataCacheBuffer, writeLoc, vec4( vec3( ( writeLoc.x ^ writeLoc.y ^ writeLoc.z ) / 255.0f ), 1.0f ) );
	// imageStore( dataCacheBuffer, writeLoc, vec4( pos, 1.0f ) );
	imageStore( dataCacheBuffer, writeLoc, color );
}

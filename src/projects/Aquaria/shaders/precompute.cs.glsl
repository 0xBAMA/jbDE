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

// ==============================================================================================
// Hash by David_Hoskins
#define UI0 1597334673U
#define UI1 3812015801U
#define UI2 uvec2(UI0, UI1)
#define UI3 uvec3(UI0, UI1, 2798796415U)
#define UIF (1.0 / float(0xffffffffU))

vec3 hash33( vec3 p ) {
	uvec3 q = uvec3( ivec3( p ) ) * UI3;
	q = ( q.x ^ q.y ^ q.z )*UI3;
	return -1.0 + 2.0 * vec3( q ) * UIF;
}

// Gradient noise by iq (modified to be tileable)
float gradientNoise( vec3 x, float freq ) {
    // grid
    vec3 p = floor( x );
    vec3 w = fract( x );
    
    // quintic interpolant
    vec3 u = w * w * w * ( w * ( w * 6.0 - 15.0 ) + 10.0 );

    // gradients
    vec3 ga = hash33( mod( p + vec3( 0.0, 0.0, 0.0 ), freq ) );
    vec3 gb = hash33( mod( p + vec3( 1.0, 0.0, 0.0 ), freq ) );
    vec3 gc = hash33( mod( p + vec3( 0.0, 1.0, 0.0 ), freq ) );
    vec3 gd = hash33( mod( p + vec3( 1.0, 1.0, 0.0 ), freq ) );
    vec3 ge = hash33( mod( p + vec3( 0.0, 0.0, 1.0 ), freq ) );
    vec3 gf = hash33( mod( p + vec3( 1.0, 0.0, 1.0 ), freq ) );
    vec3 gg = hash33( mod( p + vec3( 0.0, 1.0, 1.0 ), freq ) );
    vec3 gh = hash33( mod( p + vec3( 1.0, 1.0, 1.0 ), freq ) );
    
    // projections
    float va = dot( ga, w - vec3( 0.0, 0.0, 0.0 ) );
    float vb = dot( gb, w - vec3( 1.0, 0.0, 0.0 ) );
    float vc = dot( gc, w - vec3( 0.0, 1.0, 0.0 ) );
    float vd = dot( gd, w - vec3( 1.0, 1.0, 0.0 ) );
    float ve = dot( ge, w - vec3( 0.0, 0.0, 1.0 ) );
    float vf = dot( gf, w - vec3( 1.0, 0.0, 1.0 ) );
    float vg = dot( gg, w - vec3( 0.0, 1.0, 1.0 ) );
    float vh = dot( gh, w - vec3( 1.0, 1.0, 1.0 ) );
	
    // interpolation
    return va + 
           u.x * ( vb - va ) + 
           u.y * ( vc - va ) + 
           u.z * ( ve - va ) + 
           u.x * u.y * ( va - vb - vc + vd ) + 
           u.y * u.z * ( va - vc - ve + vg ) + 
           u.z * u.x * ( va - vb - ve + vf ) + 
           u.x * u.y * u.z * ( -va + vb + vc - vd + ve - vf - vg + vh );
}

float perlinfbm( vec3 p, float freq, int octaves ) {
    float G = exp2( -0.85 );
    float amp = 1.0;
    float noise = 0.0;
    for ( int i = 0; i < octaves; ++i ) {
        noise += amp * gradientNoise( p * freq, freq );
        freq *= 2.0;
        amp *= G;
    }
    return noise;
}

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
	const ivec3 writeLoc = ivec3( gl_GlobalInvocationID.xyz ) * 8 + offset;
	const vec3 pos = vec3( writeLoc ) + vec3( 0.5f ) - ( vec3( imageSize( dataCacheBuffer ) ) / 2.0f );

	// running color
	vec4 color = vec4( 0.0f );

	// grid
	const bool x = ( writeLoc.x + 10 ) % 55 < 4;
	const bool y = ( writeLoc.y + 10 ) % 90 < 4;
	const bool z = ( writeLoc.z + 15 ) % 32 < 3;
	if ( ( x && y ) || ( x && z ) || ( y && z ) ) {
		if ( perlinfbm( pos / 168.0f, 2.0f, 3 ) < 0.125f ) {
			color = vec4( vec3( 0.618f + perlinfbm( pos / 300.0f, 2.0f, 3 ) ), 1.0f );
		}
	}

	// iterate through the spheres
		// keep top four nearest
		// keep distance to nearest
		// keep direction away from nearest

	float minDistance = 10000.0f;
	for ( int i = 0; i < numSpheres; i++ ) {
		vec4 pr = spheres[ i ].positionRadius;
		float d = distance( pos, pr.xyz ) - pr.w;
		minDistance = min( minDistance, d );
		if ( minDistance == d && d <= 0.0f ) {
			// color = vec4( spheres[ i ].colorMaterial.rgb, clamp( -minDistance, 0.0f, 1.0f ) );
			color = spheres[ i ].colorMaterial;
		}
	}

	// write the data to the image - pack 16-bit ids into 32 bit storage
	// imageStore( idxBuffer, writeLoc, uvec4( <closest> <second>, <third> <fourth>, 0.0f, 1.0f ) );
	imageStore( dataCacheBuffer, writeLoc, color );
}

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

float freq = 19.0;
const int octaves = 2;
float noise( vec3 p ) {
	return perlinfbm( p, freq, octaves );
}
vec3 curlNoise( vec3 p ) {
// my curl noise: https://www.shadertoy.com/view/ssKczW
	vec3 col;

	// general structure from: https://al-ro.github.io/projects/embers/
	float n1, n2, a, b;
	vec2 epsilon = vec2( 0.1, 0.0 );
	n1 = noise( p + epsilon.yxy );
	n2 = noise( p - epsilon.yxy );
	a = ( n1 - n2 ) / ( 2.0 * epsilon.x );
	n1 = noise( p + epsilon.yyx );
	n2 = noise( p - epsilon.yyx );
	b = ( n1 - n2 ) / ( 2.0 * epsilon.x );
	col.x = a - b;

	n1 = noise( p + epsilon.yyx );
	n2 = noise( p - epsilon.yyx );
	a = ( n1 - n2 ) / ( 2.0 * epsilon.x );
	n1 = noise( p + epsilon.xyy );
	n2 = noise( p - epsilon.xyy );
	b = ( n1 - n2 ) / ( 2.0 * epsilon.x );
	col.y = b - a;

	n1 = noise( p + epsilon.xyy );
	n2 = noise( p - epsilon.xyy );
	a = ( n1 - n2 ) / ( 2.0 * epsilon.x );
	n1 = noise( p + epsilon.yxy );
	n2 = noise( p - epsilon.yxy );
	b = ( n1 - n2 ) / ( 2.0 * epsilon.x );
	col.z = a - b;

	// Output to screen
	// return normalize( col );
	return col;
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

// ==============================================================================================
// procedural wood + support funcs from dean the coder https://www.shadertoy.com/view/mdy3R1
///////////////////////////////////////////////////////////////////////////////
#define sat(x) clamp(x,0.,1.)
#define S(a,b,c) smoothstep(a,b,c)
#define S01(a) S(0.,1.,a)

float sum2(vec2 v) { return dot(v, vec2(1)); }
float h31(vec3 p3) {
	p3 = fract(p3 * .1031);
	p3 += dot(p3, p3.yzx + 333.3456);
	return fract(sum2(p3.xy) * p3.z);
}

float h21(vec2 p) { return h31(p.xyx); }
float n31(vec3 p) {
	const vec3 s = vec3(7, 157, 113);
	// Thanks Shane - https://www.shadertoy.com/view/lstGRB
	vec3 ip = floor(p);
	p = fract(p);
	p = p * p * (3. - 2. * p);
	vec4 h = vec4(0, s.yz, sum2(s.yz)) + dot(ip, s);
	h = mix(fract(sin(h) * 43758.545), fract(sin(h + s.x) * 43758.545), p.x);
	h.xy = mix(h.xz, h.yw, p.y);
	return mix(h.x, h.y, p.z);
}
// roughness: (0.0, 1.0], default: 0.5
// Returns unsigned noise [0.0, 1.0]
float fbm(vec3 p, int octaves, float roughness) {
	float sum = 0.,
	      amp = 1.,
	      tot = 0.;
	roughness = sat(roughness);
	while (octaves-- > 0) {
		sum += amp * n31(p);
		tot += amp;
		amp *= roughness;
		p *= 2.;
	}
	return sum / tot;
}
vec3 randomPos(float seed) {
	vec4 s = vec4(seed, 0, 1, 2);
	return vec3(h21(s.xy), h21(s.xz), h21(s.xw)) * 1e2 + 1e2;
}
// Returns unsigned noise [0.0, 1.0]
float fbmDistorted(vec3 p) {
	p += (vec3(n31(p + randomPos(0.)), n31(p + randomPos(1.)), n31(p + randomPos(2.))) * 2. - 1.) * 1.12;
	return fbm(p, 8, .5);
}
// vec3: detail(/octaves), dimension(/inverse contrast), lacunarity
// Returns signed noise.
float musgraveFbm(vec3 p, float octaves, float dimension, float lacunarity) {
	float sum = 0.,
	      amp = 1.,
	      m = pow(lacunarity, -dimension);
	while (octaves-- > 0.) {
		float n = n31(p) * 2. - 1.;
		sum += n * amp;
		amp *= m;
		p *= lacunarity;
	}
	return sum;
}
// Wave noise along X axis.
vec3 waveFbmX(vec3 p) {
	float n = p.x * 20.;
	n += .4 * fbm(p * 3., 3, 3.);
	return vec3(sin(n) * .5 + .5, p.yz);
}
///////////////////////////////////////////////////////////////////////////////
// Math
float remap01(float f, float in1, float in2) { return sat((f - in1) / (in2 - in1)); }
///////////////////////////////////////////////////////////////////////////////
// Wood material.
vec3 matWood(vec3 p) {
	float n1 = fbmDistorted(p * vec3(7.8, 1.17, 1.17));
	n1 = mix(n1, 1., .2);
	float n2 = mix(musgraveFbm(vec3(n1 * 4.6), 8., 0., 2.5), n1, .85),
	      dirt = 1. - musgraveFbm(waveFbmX(p * vec3(.01, .15, .15)), 15., .26, 2.4) * .4;
	float grain = 1. - S(.2, 1., musgraveFbm(p * vec3(500, 6, 1), 2., 2., 2.5)) * .2;
	n2 *= dirt * grain;
    
    // The three vec3 values are the RGB wood colors - Tweak to suit.
	return mix(mix(vec3(.03, .012, .003), vec3(.25, .11, .04), remap01(n2, .19, .56)), vec3(.52, .32, .19), remap01(n2, .56, 1.));
}
#undef sat
#undef S
#undef S01


void main () {
	// voxel location
	const ivec3 writeLoc = ivec3( gl_GlobalInvocationID.xyz ) + offset;
	const vec3 pos = vec3( writeLoc ) + vec3( 0.5f ) - ( vec3( imageSize( dataCacheBuffer ) ) / 2.0f );

	// running color
	vec4 color = vec4( 0.0f );

	// grid
	const bool x = ( writeLoc.x + 10 ) % 168 < 20;
	const bool y = ( writeLoc.y + 10 ) % 128 < 16;
	const bool z = ( writeLoc.z ) % 132 < 40;
	if ( ( x && y ) || ( x && z ) || ( y && z ) || writeLoc.x < 20 || writeLoc.y < 20 ) {
		if ( perlinfbm( pos / 500.0f, 2.0f, 10 ) < 0.0f || writeLoc.x < 20 || writeLoc.y < 20 ) {
			// color = vec4( vec3( 0.618f + perlinfbm( pos / 300.0f, 2.0f, 3 ) ), 1.0f );
			vec3 c = matWood( pos / 60.0f );
			color = vec4( c, 1.0f + dot( c, vec3( 0.299f, 0.587f, 0.114f ) ) ); // luma to set alpha
		}
	}

	// frame
	// const int width = 168;
	// bool skip = false;
	// if ( writeLoc.y < 100 || writeLoc.y > ( imageSize( dataCacheBuffer ).y - 100 ) || writeLoc.x < 100 ) {
	// 	if ( perlinfbm( pos / 900.0f, 2.0f, 10 ) < 0.0f ) {
	// 		vec3 c = matWood( pos / 40.0f );
	// 		color = vec4( c, 1.0f + dot( c, vec3( 0.299f, 0.587f, 0.114f ) ) ); // luma to set alpha
	// 		skip = true;
	// 	}
	// }

	// iterate through the spheres
		// keep top four nearest
		// keep distance to nearest
		// keep direction away from nearest

	// if ( !skip ) {
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
	// }

	// write the data to the image - pack 16-bit ids into 32 bit storage
	// imageStore( idxBuffer, writeLoc, uvec4( <closest> <second>, <third> <fourth>, 0.0f, 1.0f ) );
	imageStore( dataCacheBuffer, writeLoc, color );
}

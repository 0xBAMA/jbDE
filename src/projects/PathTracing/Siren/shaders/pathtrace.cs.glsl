#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( rgba32f ) uniform image2D accumulatorColor;
layout( rgba32f ) uniform image2D accumulatorNormalsAndDepth;
layout( rgba8ui ) uniform uimage2D blueNoise;

#include "hg_sdf.glsl" // SDF modeling functions
#include "twigl.glsl" // noise etc utils

// offsets
uniform ivec2 tileOffset;
uniform ivec2 noiseOffset;

// more general parameters
uniform int wangSeed;
uniform int subpixelJitterMethod;
uniform int sampleNumber;
uniform int frameNumber;
uniform float exposure;
uniform float FoV;
uniform float uvScalar;
uniform vec3 viewerPosition;
uniform vec3 basisX;
uniform vec3 basisY;
uniform vec3 basisZ;
uniform int cameraType;

// thin lens parameters
uniform bool thinLensEnable;
uniform float thinLensFocusDistance;
uniform float thinLensJitterRadius;

// raymarch parameters
uniform int raymarchMaxSteps;
uniform int raymarchMaxBounces;
uniform float raymarchMaxDistance;
uniform float raymarchEpsilon;
uniform float raymarchUnderstep;

// scene parameters
uniform vec3 skylightColor;

// CPU-generated sphere array
	// I want to generalize this:
		// primitive type
		// primitive parameters
		// material details
const int numSpheres = 0;// 5000;
struct sphere_t {
	vec4 positionRadius;
	vec4 colorMaterial;
};
layout( binding = 0, std430 ) buffer sphereData {
	sphere_t spheres[];
};

mat3 Rotate3D ( const float angle, const vec3 axis ) {
	const vec3 a = normalize( axis );
	const float s = sin( angle );
	const float c = cos( angle );
	const float r = 1.0f - c;
	return mat3(
		a.x * a.x * r + c,
		a.y * a.x * r + a.z * s,
		a.z * a.x * r - a.y * s,
		a.x * a.y * r - a.z * s,
		a.y * a.y * r + c,
		a.z * a.y * r + a.x * s,
		a.x * a.z * r + a.y * s,
		a.y * a.z * r - a.x * s,
		a.z * a.z * r + c
	);
}

#include "random.h"

vec3 GetColorForTemperature ( const float temperature ) {
	mat3 m = ( temperature <= 6500.0f )
		? mat3( vec3( 0.0f, -2902.1955373783176f, -8257.7997278925690f ),
				vec3( 0.0f, 1669.5803561666639f, 2575.2827530017594f ),
				vec3( 1.0f, 1.3302673723350029f, 1.8993753891711275f ) )
		: mat3( vec3( 1745.0425298314172f, 1216.6168361476490f, -8257.7997278925690f ),
				vec3( -2666.3474220535695f, -2173.1012343082230f, 2575.2827530017594f ),
				vec3( 0.55995389139931482f, 0.70381203140554553f, 1.8993753891711275f ) );
	return mix( clamp( vec3( m[ 0 ] / ( vec3( clamp( temperature, 1000.0f, 40000.0f ) ) + m[ 1 ] )
		+ m[ 2 ] ), vec3( 0.0f ), vec3( 1.0f ) ), vec3( 1.0f ), smoothstep( 1000.0f, 0.0f, temperature ) );
}

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

// other cool procedural materials to explore/learn from:
// marble:
	// https://www.shadertoy.com/view/MdXSzX
	// https://www.shadertoy.com/view/3sfXzB
	// https://www.shadertoy.com/view/XsKcRm
	// https://www.shadertoy.com/view/Wd33Ws

// stained surface
	// https://www.shadertoy.com/view/7sVXWD
	// https://www.shadertoy.com/view/7stSzl
	// https://www.shadertoy.com/view/Ntsczn
	// https://www.shadertoy.com/view/WljSWz


// ==============================================================================================

float Reflectance ( const float cosTheta, const float IoR ) {
	// Use Schlick's approximation for reflectance
	float r0 = ( 1.0f - IoR ) / ( 1.0f + IoR );
	r0 = r0 * r0;
	return r0 + ( 1.0f - r0 ) * pow( ( 1.0f - cosTheta ), 5.0f );
}

uvec4 BlueNoiseReference ( ivec2 location ) {
	location += noiseOffset;
	location.x = location.x % imageSize( blueNoise ).x;
	location.y = location.y % imageSize( blueNoise ).y;
	return imageLoad( blueNoise, location );
}

bool BoundsCheck ( const ivec2 loc ) {
	// used to abort off-image samples
	const ivec2 bounds = ivec2( imageSize( accumulatorColor ) ).xy;
	return ( loc.x < bounds.x && loc.y < bounds.y && loc.x >= 0 && loc.y >= 0 );
}

#define NONE	0
#define BLUE	1
#define UNIFORM	2
#define WEYL	3
#define WEYLINT	4

vec2 SubpixelOffset () {
	vec2 offset = vec2( 0.0f );
	switch ( subpixelJitterMethod ) {
		case NONE:
			offset = vec2( 0.5f );
			break;

		case BLUE:
			bool oddFrame = ( sampleNumber % 2 == 0 );
			offset = ( oddFrame ?
				BlueNoiseReference( ivec2( gl_GlobalInvocationID.xy + tileOffset ) ).xy :
				BlueNoiseReference( ivec2( gl_GlobalInvocationID.xy + tileOffset ) ).zw ) / 255.0f;
			break;

		case UNIFORM:
			offset = vec2( NormalizedRandomFloat(), NormalizedRandomFloat() );
			break;

		case WEYL:
			offset = fract( float( sampleNumber ) * vec2( 0.754877669f, 0.569840296f ) );
			break;

		case WEYLINT:
			offset = fract( vec2( sampleNumber * 12664745, sampleNumber * 9560333 ) / exp2( 24.0f ) );	// integer mul to avoid round-off
			break;

		default:
			break;
	}
	return offset;
}

struct ray {
	vec3 origin;
	vec3 direction;
};

#define NORMAL		0
#define SPHERICAL	1
#define SPHERICAL2	2
#define SPHEREBUG	3
#define SIMPLEORTHO	4
#define ORTHO		5

ray getCameraRayForUV ( vec2 uv ) { // switchable cameras ( fisheye, etc ) - Assumes -1..1 range on x and y
	const float aspectRatio = float( imageSize( accumulatorColor ).x ) / float( imageSize( accumulatorColor ).y );

	ray r;
	r.origin	= vec3( 0.0f );
	r.direction	= vec3( 0.0f );

	// apply uvScalar here, probably

	switch ( cameraType ) {
	case NORMAL:
	{
		r.origin = viewerPosition;
		r.direction = normalize( aspectRatio * uv.x * basisX + uv.y * basisY + ( 1.0f / FoV ) * basisZ );
		break;
	}

	case SPHERICAL:
	{
		uv *= uvScalar;
		uv.y /= aspectRatio;
		uv.x -= 0.05f;
		uv = vec2( atan( uv.y, uv.x ) + 0.5f, ( length( uv ) + 0.5f ) * acos( -1.0f ) );
		vec3 baseVec = normalize( vec3( cos( uv.y ) * cos( uv.x ), sin( uv.y ), cos( uv.y ) * sin( uv.x ) ) );
		// derived via experimentation, pretty close to matching the orientation of the normal camera
		baseVec = Rotate3D( PI / 2.0f, vec3( 2.5f, 0.4f, 1.0f ) ) * baseVec;

		r.origin = viewerPosition;
		r.direction = normalize( -baseVec.x * basisX + baseVec.y * basisY + ( 1.0f / FoV ) * baseVec.z * basisZ );
		break;
	}

	case SPHERICAL2:
	{
		uv *= uvScalar;
		// uv.y /= aspectRatio;

		// rotate up/down by uv.y
		mat3 rotY = Rotate3D( uv.y, basisX );
		// rotate left/right by uv.x
		mat3 rotX = Rotate3D( uv.x, basisY );

		r.origin = viewerPosition;
		r.direction = rotX * rotY * basisZ;

		break;
	}

	case SPHEREBUG:
	{
		uv *= uvScalar;
		uv.y /= aspectRatio;
		uv = vec2( atan( uv.y, uv.x ) + 0.5f, ( length( uv ) + 0.5f ) * acos( -1.0f ) );
		vec3 baseVec = normalize( vec3( cos( uv.y ) * cos( uv.x ), sin( uv.y ), cos( uv.y ) * sin( uv.x ) ) );

		r.origin = viewerPosition;
		r.direction = normalize( -baseVec.x * basisX + baseVec.y * basisY + ( 1.0f / FoV ) * baseVec.z * basisZ );
		break;
	}

	case SIMPLEORTHO: // basically a long focal length, not quite orthographic
	{	// this isn't correct - need to adjust ray origin with basisX and basisY, and set ray direction equal to basisZ
		uv.y /= aspectRatio;
		vec3 baseVec = vec3( uv * FoV, 1.0f );
		r.origin = viewerPosition;
		r.direction = normalize( basisX * baseVec.x + basisY * baseVec.y + basisZ * baseVec.z );
		break;
	}

	case ORTHO:
	{
		uv.y /= aspectRatio;
		r.origin = viewerPosition + basisX * FoV * uv.x + basisY * FoV * uv.y;
		r.direction = basisZ;
		break;
	}

	default:
		break;
	}

	if ( thinLensEnable || cameraType == SPHEREBUG ) { // or we want that fucked up split sphere behavior... sphericalFucked, something
		// thin lens adjustment
		vec3 focuspoint = r.origin + ( ( r.direction * thinLensFocusDistance ) / dot( r.direction, basisZ ) );
		// vec2 diskOffset = thinLensJitterRadius * RandomInUnitDisk();
		vec2 diskOffset = thinLensJitterRadius * RejectionSampleHexOffset();
		r.origin += diskOffset.x * basisX + diskOffset.y * basisY;
		r.direction = normalize( focuspoint - r.origin );
	}

	return r;
}

// ==============================================================================================

// organic shape
float deOrganic ( vec3 p ) {
	float S = 1.0f;
	float R, e;
	float time = 0.0f;
	p.y += p.z;
	p = vec3( log( R = length( p ) ) - time, asin( -p.z / R ), atan( p.x, p.y ) + time );
	for ( e = p.y - 1.5f; S < 6e2f; S += S ) {
		e += sqrt( abs( dot( sin( p.zxy * S ), cos( p * S ) ) ) ) / S;
	}
	return e * R * 0.1f;
}

float deOrganic2 ( vec3 p ) {
	#define V vec2( 0.7f, -0.7f )
	#define G(p) dot( p, V )
	float i = 0.0f, g = 0.0f, e = 1.0f;
	float t = 0.34f; // change to see different behavior
	for ( int j = 0; j++ < 8;){
		p = abs( Rotate3D( 0.34f, vec3( 1.0f, -3.0f, 5.0f ) ) * p * 2.0f ) - 1.0f,
		p.xz -= ( G( p.xz ) - sqrt( G( p.xz ) * G( p.xz ) + 0.05f ) ) * V;
	}
	return length( p.xz ) / 3e2f;
	#undef V
	#undef G
}

mat2 rot2 ( in float a ) { float c = cos( a ), s = sin( a ); return mat2( c, s, -s, c ); }
float deOrganic3 ( vec3 p ) {
	float d = 1e5f;
	const int n = 3;
	const float fn = float( n );
	for ( int i = 0; i < n; i++ ) {
		vec3 q = p;
		float a = float( i ) * fn * 2.422f; //*6.283/fn
		a *= a;
		q.z += float( i ) * float( i ) * 1.67f; //*3./fn
		q.xy *= rot2( a );
		float b = ( length( length( sin( q.xy ) + cos( q.yz ) ) ) - 0.15f );
		float f = max( 0.0f, 1.0f - abs( b - d ) );
		d = min( d, b ) - 0.25f * f * f;
	}
	return d;
}

float deOrganic4 ( vec3 p ) {
	p -= vec3( 0.0f, 6.0f, 0.0f );
	const int iterations = 30;
	float d = 2.0; // vary this parameter, range is like -20 to 20
	p=p.yxz;
	pR(p.yz, 1.570795);
	p.x += 6.5;
	// p.yz = mod(abs(p.yz)-.0, 20.) - 10.;
	float scale = 1.25;
	p.xy /= (1.+d*d*0.0005);

	float l = 0.;
	for (int i=0; i < iterations; i++) {
		p.xy = abs(p.xy);
		p = p*scale + vec3(-3. + d*0.0095,-1.5,-.5);
		pR(p.xy,0.35-d*0.015);
		pR(p.yz,0.5+d*0.02);
		vec3 p6 = p*p*p; p6=p6*p6;
		l =pow(p6.x + p6.y + p6.z, 1./6.);
	}
	return l*pow(scale, -float(iterations))-.15;
}

float deAsteroids ( vec3 p ) {
	float i,a,n,h,d=1.,t=0.3; // change t for different behavior
	vec3 q=vec3( 0.0f );
	n=.4;
	// for(a=1.;a<2e2;n+=q.x*q.y*q.z/a)
	for(a=1.;a<6e2;n+=q.x*q.y*q.z/a)
		p.xy*=rotate2D(a+=a),
		q=cos(p*a+t);
	return n*.3;
}

// tree shape
// mat2 rotate2D( float r ) {
	// return mat2( cos( r ), sin( r ), -sin( r ), cos( r ) );
// }
float deTree ( vec3 p ) {
	float d, a;
	d = a = 1.0f;
	for ( int j = 0; j++ < 16; ) {
		p.xz = abs( p.xz ) * rotate2D( PI / 3.0f );
		d = min( d, max( length( p.zx ) - 0.3f, p.y - 0.4f ) / a );
		p.yx *= rotate2D( 0.5f );
		p.y -= 3.0f;
		p *= 1.6f;
		a *= 1.6f;
	}
	return d;
}

float deJenga ( vec3 p ){
	vec3 P = p, Q, b = vec3( 4.0f, 2.8f, 15.0f );
	float i, d = 1.0f, a;
	Q = mod( P, b ) - b * 0.5f;
	d = P.z - 6.0f;
	a = 1.3f;
	for ( int j = 0; j++ < 17; ) {
		d = min( d, length( max( abs( Q ) - b.zyy / 13.0f, 0.0f ) ) / a );
		Q = vec3( Q.y, abs( Q.x ) - 1.0f, Q.z + 0.3f ) * 1.4f;
		a *= 1.4f;
	}
	return d;
}

float deGate ( vec3 p ) {
	float y=p.y+=.2;
	p.z-=round(p.z);
	float e, s=3.;
	p=.7-abs(p);
	for(int i=0;i++<8;p.z+=5.)
		p=abs(p.x>p.y?p:p.yxz)-.8,
		s*=e=5./min(dot(p,p),.5),
		p=abs(p)*e-6.;
	return e=min(y,length(p.yz)/s);
}

// float deLantern ( vec3 p ) {
//   float e,v,u;
//   e = v = 2;
//   for(int j=0;j++<12;j>3?e=min(e,length(p.xz+length(p)/u*.55)/v-.006),p.xz=abs(p.xz)-.7,p:p=abs(p)-.86)
//     v /= u = dot( p, p ),
//     p /= u,
//     p.y = 1.7 - p.y;
//   return e;
// }

 mat2 rot(float a) {
    return mat2(cos(a),sin(a),-sin(a),cos(a));
  }
  vec4 formula(vec4 p) {
    p.xz = abs(p.xz+1.)-abs(p.xz-1.)-p.xz;
    p=p*2./clamp(dot(p.xyz,p.xyz),.15,1.)-vec4(0.5,0.5,0.8,0.);
    p.xy*=rot(.5);
    return p;
  }
  float screen(vec3 p) {
    float d1=length(p.yz-vec2(.25,0.))-.5;
    float d2=length(p.yz-vec2(.25,2.))-.5;
    return min(max(d1,abs(p.x-.3)-.01),max(d2,abs(p.x+2.3)-.01));
  }
  float deGrail ( vec3 pos ) {
    vec3 tpos=pos;
    tpos.z=abs(2.-mod(tpos.z,4.));
    vec4 p=vec4(tpos,1.5);
    float y=max(0.,.35-abs(pos.y-3.35))/.35;

    for (int i=0; i<8; i++) {p=formula(p);}
    float fr=max(-tpos.x-4.,(length(max(vec2(0.),p.yz-3.)))/p.w);

    float sc=screen(tpos);
    return min(sc,fr);
  }

float deFractal ( vec3 p ) {
	float s = 2.0f, l = 0.0f;
	p = abs( p );
	for ( int j = 0; j++ < 8; )
		p = 1.0f - abs( abs( p - 2.0f ) - 1.0f ),
		p *= l = 1.2f / dot( p, p ), s *= l;
	return dot( p, normalize( vec3( 3.0f, -2.0f, -1.0f ) ) ) / s;
}

float deFractal2 ( vec3 p ) {
	float s=3., offset=8., e=0.0f;
	for(int i=0;i++<9;p=vec3(2,4,2)-abs(abs(p)*e-vec3(4,4,2)))
		s*=e=max(1.,(8.+offset)/dot(p,p));
	return min(length(p.xz),p.y)/s;
}

// tdhooper variant 1 - spherical inversion
vec2 wrap ( vec2 x, vec2 a, vec2 s ) {
  x -= s;
  return (x - a * floor(x / a)) + s;
}

void TransA ( inout vec3 z, inout float DF, float a, float b ) {
  float iR = 1. / dot( z, z );
  z *= -iR;
  z.x = -b - z.x;
  z.y = a + z.y;
  DF *= iR; // max( 1.0, iR );
}

float deFractal3 ( vec3 z ) {
  vec3 InvCenter = vec3( 0.0, 1.0, 1.0 );
  float rad = 0.8;
  float KleinR = 1.5 + 0.39;
  float KleinI = ( 0.55 * 2.0 - 1.0 );
  vec2 box_size = vec2( -0.40445, 0.34 ) * 2.0;
  vec3 lz = z + vec3( 1.0 ), llz = z + vec3( -1.0 );
  float d = 0.0; float d2 = 0.0;
  z = z - InvCenter;
  d = length( z );
  d2 = d * d;
  z = ( rad * rad / d2 ) * z + InvCenter;
  float DE = 1e12;
  float DF = 1.0;
  float a = KleinR;
  float b = KleinI;
  float f = sign( b ) * 0.45;
  for ( int i = 0; i < 100; i++ ) {
    z.x += b / a * z.y;
    z.xz = wrap( z.xz, box_size * 2.0, -box_size );
    z.x -= b / a * z.y;
    if ( z.y >= a * 0.5 + f * ( 2.0 * a - 1.95 ) / 4.0 * sign( z.x + b * 0.5 ) * 
     ( 1.0 - exp( -( 7.2 - ( 1.95 - a ) * 15.0 )* abs(z.x + b * 0.5 ) ) ) ) {
      z = vec3( -b, a, 0.0 ) - z;
    } //If above the separation line, rotate by 180° about (-b/2, a/2)
    TransA( z, DF, a, b ); //Apply transformation a
    if ( dot( z - llz, z - llz ) < 1e-5 ) {
      break;
    } //If the iterated points enters a 2-cycle, bail out
    llz = lz; lz = z; //Store previous iterates
  }
  float y =  min(z.y, a - z.y);
  DE = min( DE, min( y, 0.3 ) / max( DF, 2.0 ) );
  DE = DE * d2 / ( rad + d * DE );
  return DE;
}

float deApollo ( vec3 p ) {
	float s = 3.0f, e;
	for ( int i = 0; i++ < 10; )
		p =mod( p - 1.0f, 2.0f ) - 1.0f,
		s *= e = 1.4f / dot( p, p ),
		p *= e;
	return length( p.yz ) / s;
}

float deStairs ( vec3 P ) {
	vec3 Q;
	float a, d = min( ( P.y - abs( fract( P.z ) - 0.5f ) ) * 0.7f, 1.5f - abs( P.x ) );
	for ( a = 2.0f; a < 6e2f; a += a ) {
		Q = P * a;
		Q.xz *= rotate2D( a );
		d += abs( dot( sin( Q ), Q - Q + 1.0f ) ) / a / 7.0f;
	}
	return d;
}

float deWhorl ( vec3 p ) {
	float i, e, s, g, k = 0.01f;
	p.xy *= mat2( cos( p.z ), sin( p.z ), -sin( p.z ), cos( p.z ) );
	e = 0.3f - dot( p.xy, p.xy );
	for( s = 2.0f; s < 2e2f; s /= 0.6f ) {
		p.yz *= mat2( cos( s ), sin( s ), -sin( s ), cos( s ) );
		e += abs( dot( sin( p * s * s * 0.2f ) / s, vec3( 1.0f ) ) );
	}
	return e;
}

float hash(vec2 p) { return fract(sin(dot(p, vec2(123.45, 875.43))) * 5432.3); }
float noise(vec2 p) {
	vec2 i = floor(p),
	     f = fract(p);
	float a = hash(i),
	      b = hash(i + vec2(1, 0)),
	      c = hash(i + vec2(0, 1)),
	      d = hash(i + vec2(1));
	f = f * f * (3. - 2. * f);
	return mix(a, b, f.x) + (c - a) * f.y * (1. - f.x) + (d - b) * f.x * f.y;
}
float fbm(vec2 p) {
	float f = 0.;
	f += .5 * noise(p * 1.1);
	f += .22 * noise(p * 2.3);
	f += .155 * noise(p * 3.9);
	f += .0625 * noise(p * 8.4);
	f += .03125 * noise(p * 15.);
	return f;
}
float rfbm(vec2 xz) { return abs(2. * fbm(xz) - 1.); }
float sdTerrain(vec3 p) {
	if (p.y > 0.) return 1e10;
	float h = rfbm(p.xz * .2);
	p.xz += vec2(1);
	h += .5 * rfbm(p.xz * .8);
	h += .25 * rfbm(p.xz * 2.);
	h += .03 * rfbm(p.xz * 16.1);
	h *= .7 * fbm(p.xz);
	h -= .7;
	return abs(p.y - h) * .6;
}

// #define rot(a) mat2(cos(a),sin(a),-sin(a),cos(a))
// #define sabs(x)sqrt((x)*(x)+.005)
// #define sabs2(x)sqrt((x)*(x)+1e-4)
// #define smax(a,b) (a+b+sabs2(a-b))*.5
// void sfold90(inout vec2 p){
//     p=(p.x+p.y+vec2(1,-1)*sabs(p.x-p.y))*.5;
// }
// float de ( vec3 p ) {
//     vec3 q=p;
//     p=abs(p)-4.;
//     sfold90(p.xy);
//     sfold90(p.yz);
//     sfold90(p.zx);
// 	float s=2.5;
// 	p=sabs(p);
// 	vec3  p0 = p*1.5;
// 	for (float i=0.; i<4.; i++){
//     	p=1.-sabs2(sabs2(p-2.)-1.); 
//     	float g=-5.5*clamp(.7*smax(1.6/dot(p,p),.7),.0,5.5);
//     	p*=g;
//     	p+=p0+normalize(vec3(1,5,12))*(5.-.8*i);
//         s*=g;
// 	}
// 	s=sabs(s);
// 	float a=25.;
// 	p-=clamp(p,-a,a);
// 	q=abs(q)-vec3(3.7);
//     sfold90(q.xy);
//     sfold90(q.yz);
//     sfold90(q.zx);
//   	return smax(max(abs(q.y),abs(q.z))-1.3,length(p)/s-.00);
// }

// ==============================================================================================
// ==============================================================================================

#define NOHIT						0
#define EMISSIVE					1
#define DIFFUSE						2
#define DIFFUSEAO					3
#define METALLIC					4
#define RAINBOW						5
#define MIRROR						6
#define PERFECTMIRROR				7
#define WOOD						8
#define MALACHITE					9
#define CHECKER						10
#define REFRACTIVE					11
#define REFRACTIVE_FROSTED			12

// we're only going to do refraction on explicit intersections this time, to simplify the logic
	// objects shouldn't have this material, it is used in the explicit intersection logic / bounce behavior
#define REFRACTIVE_BACKFACE			100
#define REFRACTIVE_FROSTED_BACKFACE	101

float baseIOR = 1.0f / 1.4f;
// float baseIOR = 1.4f;

int hitPointSurfaceType = NOHIT;
vec3 hitPointColor = vec3( 0.0f );

// ==============================================================================================
// ==============================================================================================

// overal distance estimate function - the "scene"
	// hitPointSurfaceType gives the type of material
	// hitPointColor gives the albedo of the material

float de ( vec3 p ) {
	// init nohit, far from surface, no diffuse color
	hitPointSurfaceType = NOHIT;
	float sceneDist = 1000.0f;
	hitPointColor = vec3( 0.0f );

	// return sceneDist;

	// cache initial point location
	const vec3 pCache = p;
	pModPolar( p.xz, 4 );
	pMirror( p.y, -2.0f );
	p.y = 5.0f - p.y;

	// light bar
	const float dLight = fBox( p - vec3( 14.0f, 0.0f, 0.0f ), vec3( 0.15f, 0.15f, 18.0f ) );
	sceneDist = min( dLight, sceneDist );
	if ( sceneDist == dLight && dLight <= raymarchEpsilon ) {
		hitPointColor = vec3( 3.0f );
		hitPointSurfaceType = EMISSIVE;
	}

	// outer frame
	const float heightSplit = 2.0f;
	const float dFloor = fBox( p - vec3( 0.0f, -heightSplit, 0.0f ), vec3( 15.0f, 0.2f, 15.0f ) );
	const float dMolding = deGrail( Rotate3D( PI / 4.0f, vec3( 0.0f, 0.0f, 1.0f ) ) * Rotate3D( PI, vec3( 0.0f, 1.0f, 0.0f ) ) * ( 0.618f * ( p - vec3( 15.0f, 0.0f, 0.0f ) ) ) ) / 0.618f;
	pModInterval1( p.z, 3.0f, -3, 3 );
	const float dCutout = fSphere( p - vec3( 9.0f, heightSplit + 2.0f, 0.0f ), 1.618f );
	const float dPillars = max( fCylinder( p - vec3( 9.0f, 0.0f, 0.0f ), 0.618f, heightSplit + 0.5f ), -dCutout );
	const float dReflector = fOpUnionRound( fOpIntersectionChamfer( dPillars, fSphere( p - vec3( 9.0f, heightSplit + 2.0f, 0.0f ), 1.8f ), 0.1f ), fCylinder( p - vec3( 9.0f, 0.0f, 0.0f ), 0.1618f, 10.0f ), 0.3f );
	const float dOrbs = fSphere( p - vec3( 9.0f, heightSplit + 1.3f, 0.0f ), 0.618f );
	const float dFramePlusPlatform = fOpUnionRound( min( dMolding, dPillars ), dFloor, 0.5f );

	sceneDist = min( dFramePlusPlatform, sceneDist );
	if ( sceneDist == dFramePlusPlatform && dFramePlusPlatform <= raymarchEpsilon ) {
		hitPointSurfaceType = CHECKER;

		// hitPointSurfaceType = DIFFUSE;
		// hitPointColor = vec3( 0.1f, 0.3f, 0.9f );

		// hitPointSurfaceType = WOOD;

		// if ( NormalizedRandomFloat() < 0.9f ) {
		// 	hitPointSurfaceType = DIFFUSE;
		// 	hitPointColor = vec3( 0.01618f );
		// } else {
		// 	hitPointSurfaceType = MIRROR;
		// }
	}

	sceneDist = min( dOrbs, sceneDist );
	if ( sceneDist == dOrbs && dOrbs <= raymarchEpsilon ) {
		hitPointColor = vec3( 3.0f );
		hitPointSurfaceType = EMISSIVE;
	}

	sceneDist = min( dReflector, sceneDist );
	if ( sceneDist == dReflector && dReflector <= raymarchEpsilon ) {

		// hitPointSurfaceType = DIFFUSE;
		// hitPointColor = vec3( 1.0f, 0.1f, 0.1f );

		hitPointSurfaceType = MIRROR;
		// hitPointSurfaceType = PERFECTMIRROR;
	}

	return sceneDist;
}


// ==============================================================================================
// ray scattering functions

vec3 HenyeyGreensteinSampleSphere ( const vec3 n, const float g ) {
	float t = ( 0.5f - g *0.5) / ( 0.5f - g + 0.5f * g * NormalizedRandomFloat() );
	float cosTheta = ( 1.0f + g * g - t ) / ( 2.0f * g );
	float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
	float phi = 2.0f * 3.14159f * NormalizedRandomFloat();

	vec3 xyz = vec3( cos( phi ) * sinTheta, sin( phi ) * sinTheta, cosTheta );
	vec3 W = ( abs(n.x) > 0.99f ) ? vec3( 0.0f, 1.0f, 0.0f ) : vec3( 1.0f, 0.0f, 0.0f );
	vec3 N = n;
	vec3 T = normalize( cross( N, W ) );
	vec3 B = cross( T, N );
	return normalize( xyz.x * T + xyz.y * B + xyz.z * N );
}

vec3 SimpleRayScatter ( const vec3 n ) {
	if ( NormalizedRandomFloat() < 0.001f ) {
		return normalize( n + 0.4f * RandomUnitVector() );
	}
}

// ==============================================================================================
// raymarches to the next hit
// another interesting one, potentially good for a bit of speedup - https://www.shadertoy.com/view/cdsGRs
// ==============================================================================================
float Raymarch ( const vec3 origin, vec3 direction ) {
	float dQuery = 0.0f;
	float dTotal = 0.0f;
	vec3 pQuery = origin;

	for ( int steps = 0; steps < raymarchMaxSteps; steps++ ) {
		pQuery = origin + dTotal * direction;
		dQuery = de( pQuery );
		dTotal += dQuery * raymarchUnderstep;
		if ( dTotal > raymarchMaxDistance || abs( dQuery ) < raymarchEpsilon ) {
			break;
		}

		// certain chance to scatter in a random direction, per step - one of Nameless' methods for fog
		// direction = SimpleRayScatter( direction );

		// another method for volumetrics, using a phase function - doesn't work right now because of how pQuery is calculated
		// direction = HenyeyGreensteinSampleSphere( direction.xzy, 0.1f ).xzy;

	}
	return dTotal;
}
// ==============================================================================================
// raymarch-derived functions
// ==============================================================================================
// normal derived from the gradient of the SDF
vec3 Normal ( const vec3 position ) { // three methods - first one seems most practical
	vec2 e = vec2( raymarchEpsilon, 0.0f );
	return normalize( vec3( de( position ) ) - vec3( de( position - e.xyy ), de( position - e.yxy ), de( position - e.yyx ) ) );

	// vec2 e = vec2( 1.0f, -1.0f ) * raymarchEpsilon;
	// return normalize( e.xyy * de( position + e.xyy ) + e.yyx * de( position + e.yyx ) + e.yxy * de( position + e.yxy ) + e.xxx * de( position + e.xxx ) );

	// vec2 e = vec2( raymarchEpsilon, 0.0f );
	// return normalize( vec3( de( position + e.xyy ) - de( position - e.xyy ), de( position + e.yxy ) - de( position - e.yxy ), de( position + e.yyx ) - de( position - e.yyx ) ) );
}
// ==============================================================================================
// fake AO, computed from SDF
// ==============================================================================================
float CalcAO ( const vec3 position, const vec3 normal ) {
	float occ = 0.0f;
	float sca = 1.0f;
	for( int i = 0; i < 5; i++ ) {
		float h = 0.001f + 0.15f * float( i ) / 4.0f;
		float d = de( position + h * normal );
		occ += ( h - d ) * sca;
		sca *= 0.95f;
	}
	return clamp( 1.0f - 1.5f * occ, 0.0f, 1.0f );
}

// // alternate version from https://www.shadertoy.com/view/MsySWK
// float calculateAO(in vec3 pos, in vec3 nor) {
// 	float sca = 3., occ = 0.;
//     for(int i=0; i<5; i++){
//         float hr = .01 + float(i)*.5/4.;
//         float dd = map(nor * hr + pos);
//         occ += (hr - dd)*sca;
//         sca *= 0.7;
//     }
//     return clamp(1.0 - occ, 0., 1.);
// }

// ==============================================================================================
// explicit intersection primitives
struct Intersection {
	vec4 a;  // distance and normal at entry
	vec4 b;  // distance and normal at exit
};

// false intersection
const Intersection kEmpty = Intersection(
	vec4( 1e20f, 0.0f, 0.0f, 0.0f ),
	vec4( -1e20f, 0.0f, 0.0f, 0.0f )
);

bool IsEmpty ( Intersection i ) {
	return i.b.x < i.a.x;
}

Intersection IntersectSphere ( in vec3 origin, in vec3 direction, in vec3 center, float radius ) {
	// https://iquilezles.org/articles/intersectors/
	vec3 oc = origin - center;
	float b = dot( oc, direction );
	float c = dot( oc, oc ) - radius * radius;
	float h = b * b - c;
	if ( h < 0.0f )
		return kEmpty; // no intersection
	h = sqrt( h );

	// h is known to be positive at this point, b+h > b-h
	float nearHit = -b - h; vec3 nearNormal = normalize( ( origin + direction * nearHit ) - center );
	float farHit  = -b + h; vec3 farNormal  = normalize( ( origin + direction * farHit ) - center );

	return Intersection( vec4( nearHit, nearNormal ), vec4( farHit, farNormal ) );
}

// broken for some reason, tbd
Intersection IntersectBox ( in vec3 origin, in vec3 direction, in vec3 center, in vec3 size ) {
	vec3 oc = origin - center;
	vec3 m = 1.0f / direction;
	vec3 k = vec3( direction.x >= 0.0f ? size.x : -size.x, direction.y >= 0.0f ? size.y : -size.y, direction.z >= 0.0f ? size.z : -size.z );
	vec3 t1 = ( -oc - k ) * m;
	vec3 t2 = ( -oc + k ) * m;
	float tN = max( max( t1.x, t1.y ), t1.z );
	float tF = min( min( t2.x, t2.y ), t2.z );
	if ( tN > tF || tF < 0.0f ) return kEmpty;
	return Intersection(
		vec4( tN, normalize( -sign( direction ) * step( vec3( tN ), t1 ) ) ),
		vec4( tF, normalize( -sign( direction ) * step( t2, vec3( tF ) ) ) ) );
}

// just solve for t, < ro+t*d, nor > - k = 0
Intersection iPlane ( in vec3 ro, in vec3 rd, vec4 pla ) {
	float k1 = dot( ro, pla.xyz );
	float k2 = dot( rd, pla.xyz );
	float t = ( pla.w - k1 ) / k2;
	vec2 ab = ( k2 > 0.0f ) ? vec2( t, 1e20f ) : vec2( -1e20f, t );
	return Intersection( vec4( ab.x, -pla.xyz ), vec4( ab.y, pla.xyz ) );
}


// The MIT License
// Copyright © 2022 Inigo Quilez
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions: The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

// Raytracing boolean operations on shapes.
// https://www.shadertoy.com/view/mlfGRM
//
// This works by doing boolean operations in "ray space" (ie,
// manipulating intervals of ray distances). I'm only tracking one
// segment, more should be added for deeper boolean trees.
//
// When intersecting two solid intervals A and B, there are 6 possible
// scenarios for their combination:
//
// 1     x---x  |             |
// 2     x------|---x         |
// 3     x------|-------------|--x
// 4            |   x-----x   |
// 5            |   x---------|--x
// 6            |             |  x---x 
//
// Subtraction in scenario 4 produces TWO segments, but I'm only tracking
// ONE segment at a time in this shader, so I've got to take an arbitrary
// decision as to what to do in that case (line 58).

Intersection opIntersection ( Intersection a, Intersection b, out int r ) {
	if ( a.a.x < b.a.x ) {
		/* 1 */ if ( a.b.x < b.a.x ) return kEmpty;
		/* 2 */ if ( a.b.x < b.b.x ) { r=1; return Intersection( b.a, a.b ); }
		/* 3 */ { r=1; return b; }
	} else if( a.a.x < b.b.x ) {
		/* 4 */ if ( a.b.x < b.b.x ) { r=0; return a; }
		/* 5 */ { r=0; return Intersection( a.a, b.b ); }
	} else {
		/* 6 */ return kEmpty;
	}
}

Intersection opSubtraction ( Intersection b, Intersection a, out int r ) {
	if ( a.a.x < b.a.x ) {
		/* 1 */ if ( a.b.x < b.a.x ) { r=0; return b; }
		/* 2 */ if ( a.b.x < b.b.x ) { r=1; return Intersection( a.b, b.b ); }
		/* 3 */ return kEmpty;
	} else if ( a.a.x < b.b.x ) {
		/* 4 */ if ( a.b.x < b.b.x ) { r=0; return Intersection( b.a, a.a ); } // hm.... difficult to choose
		/* 5 */ { r=0; return Intersection( b.a, a.b ); }
	} else {
		/* 6 */ { r=0; return b; }
	}
}

// ==============================================================================================
struct sceneIntersection {
	float dTravel;
	vec3 normal;
	vec3 color;
	int material;

	Intersection i;
};

sceneIntersection ExplicitSceneIntersection ( in vec3 origin, in vec3 direction ) {
	// something's wrong here, I can't seem to get the behavior I'm wanting from the intersection of two spheres
	// return opIntersection( IntersectSphere( origin, direction, vec3( offset, 0.0f, 0.0f ), radius ), IntersectSphere( origin, direction, vec3( -offset, 0.0f, 0.0f ), radius ), r );

	Intersection iResult = kEmpty;
	int indexOfHit = -1;
	float nearestOverallHit = 1000000.0f;
	for ( int i = 30; i < numSpheres; i++ ) {
		Intersection current = IntersectSphere( origin, direction, spheres[ i ].positionRadius.xyz, spheres[ i ].positionRadius.w );
		// Intersection current = ( i % 2 == 0 ) ? IntersectBox( origin, direction, spheres[ i ].positionRadius.xyz, vec3( spheres[ i ].positionRadius.w ) ) : IntersectSphere( origin, direction, spheres[ i ].positionRadius.xyz, spheres[ i ].positionRadius.w );
		const float currentNearestPositive = ( current.a.x < 0.0f ) ? ( current.b.x < 0.0f ) ? 1000000.0f : current.b.x : current.a.x;
		nearestOverallHit = min( currentNearestPositive, nearestOverallHit );
		if ( currentNearestPositive == nearestOverallHit ) {
			iResult = current;
			indexOfHit = i;
		}
	}

	sceneIntersection result;
	result.dTravel = nearestOverallHit;
	result.normal = ( iResult.a.x == nearestOverallHit ) ? iResult.a.yzw : iResult.b.yzw;
	result.i = iResult;
	result.material = int( spheres[ indexOfHit ].colorMaterial.w );
	result.color = spheres[ indexOfHit ].colorMaterial.xyz;


	// // trying to debug the issues with the refractive box - why is it black?
	// Intersection iResult = IntersectBox( origin, direction, vec3( 0.0f ), vec3( 1.0f ) );
	// sceneIntersection result;
	// result.dTravel =  ( iResult.a.x < 0.0f ) ? ( iResult.b.x < 0.0f ) ? 1000000.0f : iResult.b.x : iResult.a.x;
	// result.normal = ( iResult.a.x == result.dTravel ) ? iResult.a.yzw : iResult.b.yzw;
	// result.i = iResult;
	// result.material = DIFFUSE;
	// result.color = vec3( 0.618f );


	// placeholder no-op
	// sceneIntersection result;
	// result.i = kEmpty;

	// colors > 1 really only make sense for lights
	if ( result.material != EMISSIVE )
		result.color = clamp( result.color, vec3( 0.0f ), vec3( 1.0f ) );

	return result;
}

// ==============================================================================================
sceneIntersection GetNearestSceneIntersection ( in vec3 origin, in vec3 direction ) {
	sceneIntersection result;

	// get the explict intersection result
	sceneIntersection explicitResult = ExplicitSceneIntersection( origin, direction );
	if ( IsEmpty( explicitResult.i ) || ( explicitResult.i.a.x < 0.0f && explicitResult.i.b.x < 0.0f ) ) {
		result.dTravel = raymarchMaxDistance + 1.0f;
		result.normal = vec3( 0.0f );
		result.color = vec3( 0.0f );
		result.material = NOHIT;
	} else if ( explicitResult.i.a.x < 0.0f && explicitResult.i.b.x >= 0.0f ) {
		result.dTravel = explicitResult.i.b.x;
		result.normal = explicitResult.i.b.yzw;
		result.color = explicitResult.color;
		result.material = ( explicitResult.material == REFRACTIVE ) ? REFRACTIVE_BACKFACE : ( explicitResult.material == REFRACTIVE_FROSTED ) ? REFRACTIVE_FROSTED_BACKFACE : explicitResult.material;
	} else {
		result.dTravel = explicitResult.i.a.x;
		result.normal = explicitResult.i.a.yzw;
		result.color = explicitResult.color;
		result.material = explicitResult.material;
	}

	// get the raymarch intersection result
	const float raymarchDistance = Raymarch( origin, direction );

	// compare distances and make a determination on what got hit first
	if ( raymarchDistance < result.dTravel && raymarchDistance < raymarchMaxDistance ) {
		// take the raymarch result, as it's closer
		result.dTravel = raymarchDistance;
		result.color = hitPointColor;
		result.material = hitPointSurfaceType;
		result.normal = Normal( origin + raymarchDistance * direction );
	}

	// if they both escape, we take the nohit result from the first if at the top of the function
	// if the explicit result is closer than the raymarch result, we already have the correct information in the result struct

	// return the struct with the correct data
	return result;
}

// ==============================================================================================

vec3 ColorSample ( const vec2 uvIn ) {

	// compute initial ray origin, direction
	const ray r = getCameraRayForUV( uvIn );
	const vec3 rayOrigin_initial = r.origin;
	const vec3 rayDirection_initial = r.direction;

	// use this to get first hit depth/normal here? tbd

	// variables for current and previous ray origin, direction
	vec3 rayOrigin, rayOriginPrevious;
	vec3 rayDirection, rayDirectionPrevious;

	// intial values
	rayOrigin = rayOrigin_initial;
	rayDirection = rayDirection_initial;

	// pathtracing accumulators
	vec3 finalColor = vec3( 0.0f );
	vec3 throughput = vec3( 1.0f );

	// loop over bounces
	for ( int bounce = 0; bounce < raymarchMaxBounces; bounce++ ) {

		// ==== SDF + EXPLICIT ===============================================================
		sceneIntersection result = GetNearestSceneIntersection( rayOrigin, rayDirection );

		// get previous direction, origin
		rayOriginPrevious = rayOrigin;
		rayDirectionPrevious = rayDirection;

		// update new ray origin ( at hit point )
		rayOrigin = rayOriginPrevious + result.dTravel * rayDirectionPrevious;

		// epsilon bump, along the normal vector
		if ( result.material != REFRACTIVE && result.material != REFRACTIVE_BACKFACE )
			rayOrigin += 2.0f * raymarchEpsilon * result.normal;

		// precalculate reflected vector, random diffuse vector, random specular vector
		const vec3 reflectedVector = reflect( rayDirectionPrevious, result.normal );
		const vec3 randomVectorDiffuse = normalize( ( 1.0f + raymarchEpsilon ) * result.normal + RandomUnitVector() );
		const vec3 randomVectorSpecular = normalize( ( 1.0f + raymarchEpsilon ) * result.normal + mix( reflectedVector, RandomUnitVector(), 0.1f ) );

		// ===================================================================================

		// add a check for points that are not within epsilon? just ran out the steps?
			// this is much less likely with a 0.9 understep, rather than 0.618

		if ( result.dTravel >= raymarchMaxDistance ) {

			// ray escaped - break out of loop, since you will not bounce
			finalColor += throughput * skylightColor;
			break;

		} else {

			// material specific behavior
			switch ( result.material ) {
			case NOHIT:
				break;

			case EMISSIVE:
			{
				// light emitting material
				finalColor += throughput * result.color;
				break;
			}

			case DIFFUSE:
			{
				// diffuse material
				throughput *= result.color;
				rayDirection = randomVectorDiffuse;
				break;
			}

			case DIFFUSEAO:
			{
				// diffuse material with raymarch-style ao scale
				// result.color *= pow( CalcAO( rayOrigin, result.normal ), 5.0f );
				result.color *= pow( CalcAO( rayOrigin, result.normal ), 2.0f );
				throughput *= result.color;
				// finalColor += vec3( 1.0f / CalcAO( rayOrigin, result.normal ), 0.0f, 0.0f ) + throughput;
				rayDirection = randomVectorDiffuse;
				break;
			}

			case METALLIC:
			{
				// specular material
				throughput *= result.color;
				rayDirection = randomVectorSpecular;
				break;
			}

			case RAINBOW:
			{
				// throughput *= ( result.normal + 1.0f ) / 2.0f;
				throughput *= clamp( result.normal, 0.1618f, 1.0f );
				rayDirection = randomVectorDiffuse;
				break;
			}

			case MIRROR:
			{	// perfect mirror ( slight attenuation )
				throughput *= 0.618f;
				rayDirection = reflectedVector;
				break;
			}

			case PERFECTMIRROR:
			{	// less attenuation than MIRROR - in particular, useful for lighting fixtures
				throughput *= 0.95f;
				rayDirection = reflectedVector;
				break;
			}

			case WOOD:
			{
				if ( NormalizedRandomFloat() < 0.01f ) { // mirror behavior
					rayDirection = reflectedVector;
				} else {
					throughput *= matWood( rayOrigin / 10.0f );
					rayDirection = randomVectorDiffuse;
				}
				break;
			}

			case MALACHITE:
			{
				if ( NormalizedRandomFloat() < 0.1f ) {
					rayDirection = reflectedVector;
				} else {
					throughput *= matWood( rayOrigin ).brg; // this swizzle makes a nice light green / dark green mix
					rayDirection = randomVectorDiffuse;
				}
				break;
			}

			case CHECKER:
			{
				// diffuse material
				const float thresh = 0.01;
				const float thresh2 = 0.005;
				vec3 p = rayOrigin;

				bool redLines = (
					// thick lines at zero
					( abs( p.x ) < thresh ) ||
					( abs( p.y ) < thresh ) ||
					( abs( p.z ) < thresh ) ||
					// thinner lines every 5
					( abs( mod( p.x, 5.0f ) ) < thresh2 ) ||
					( abs( mod( p.y, 5.0f ) ) < thresh2 ) ||
					( abs( mod( p.z, 5.0f ) ) < thresh2 ) );

				// the checkerboard
				bool blackOrWhite = ( step( 0.0f,
					cos( PI * p.x + PI / 2.0f ) *
					cos( PI * p.y + PI / 2.0f ) *
					cos( PI * p.z + PI / 2.0f ) ) == 0 );

				vec3 color = redLines ? vec3( 1.0f, 0.1f, 0.1f ) : ( blackOrWhite ? vec3( 0.618f ) : vec3( 0.1618f ) );
				throughput *= color;
				rayDirection = ( blackOrWhite || redLines ) ? randomVectorDiffuse : reflectedVector;
				break;
			}

			// note for refractive surfaces, we have already bumped the ray origin by 2.0 * epsilon * normal
			case REFRACTIVE:
			{
				rayOrigin -= 2.0f * raymarchEpsilon * result.normal;
				float cosTheta = min( dot( -normalize( rayDirection ), result.normal ), 1.0f );
				float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
				bool cannotRefract = ( baseIOR * sinTheta ) > 1.0f; // accounting for TIR effects

				// adding noise to IOR? interesting idea, maybe, adds visual vaiations
				// float IoRLocal = baseIOR + snoise3D( rayOrigin * 10.0f ) * 0.3f;
				// bool cannotRefract = ( IoRLocal * sinTheta ) > 1.0f;
				
				// disabling this disables first surface reflections
				if ( cannotRefract || Reflectance( cosTheta, baseIOR ) > NormalizedRandomFloat() ) {
					rayDirection = reflect( normalize( rayDirection ), result.normal );
				} else {
					rayDirection = refract( normalize( rayDirection ), result.normal, baseIOR );
					// rayDirection = refract( normalize( rayDirection ), result.normal, IoRLocal ); // for adding IOR noise
				}
				break;
			}

			case REFRACTIVE_BACKFACE:
			{
				rayOrigin += 2.0f * raymarchEpsilon * result.normal;
				result.normal = -result.normal;
				float adjustedIOR = 1.0f / baseIOR;
				float cosTheta = min( dot( -normalize( rayDirection ), result.normal ), 1.0f );
				float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
				bool cannotRefract = ( adjustedIOR * sinTheta ) > 1.0f; // accounting for TIR effects
				if ( cannotRefract || Reflectance( cosTheta, adjustedIOR ) > NormalizedRandomFloat() ) {
					rayDirection = reflect( normalize( rayDirection ), result.normal );
				} else {
					rayDirection = refract( normalize( rayDirection ), result.normal, adjustedIOR );
				}
				break;
			}

			case REFRACTIVE_FROSTED:
			{
				rayOrigin -= 2.0f * raymarchEpsilon * result.normal;
				float cosTheta = min( dot( -normalize( rayDirection ), result.normal ), 1.0f );
				float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
				bool cannotRefract = ( baseIOR * sinTheta ) > 1.0f; // accounting for TIR effects
				if ( cannotRefract || Reflectance( cosTheta, baseIOR ) > NormalizedRandomFloat() ) {
					rayDirection = normalize( mix( reflect( normalize( rayDirection ), result.normal ), RandomUnitVector(), 0.03f ) );
				} else {
					rayDirection = normalize( mix( refract( normalize( rayDirection ), result.normal, baseIOR ), RandomUnitVector(), 0.01f ) );
				}
				break;
			}

			case REFRACTIVE_FROSTED_BACKFACE:
			{
				rayOrigin += 2.0f * raymarchEpsilon * result.normal;
				result.normal = -result.normal;
				float adjustedIOR = 1.0f / baseIOR;
				float cosTheta = min( dot( -normalize( rayDirection ), result.normal ), 1.0f );
				float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
				bool cannotRefract = ( adjustedIOR * sinTheta ) > 1.0f; // accounting for TIR effects
				if ( cannotRefract || Reflectance( cosTheta, adjustedIOR ) > NormalizedRandomFloat() ) {
					rayDirection = normalize( mix( reflect( normalize( rayDirection ), result.normal ), RandomUnitVector(), 0.03f ) );
				} else {
					rayDirection = normalize( mix( refract( normalize( rayDirection ), result.normal, baseIOR ), RandomUnitVector(), 0.01f ) );
				}
				break;
			}
			}
		}

		// russian roulette termination
		// if ( hitpointSurfaceType_cache != REFRACTIVE ) {
			// russian roulette termination - chance for ray to quit early
			float maxChannel = max( throughput.r, max( throughput.g, throughput.b ) );
			if ( NormalizedRandomFloat() > maxChannel ) {
				break;
			}
			throughput *= 1.0f / maxChannel; // russian roulette compensation term
		// }

	}

	return finalColor * exposure;
}

// ==============================================================================================

void main () {
	// tiled offset
	const uvec2 location = gl_GlobalInvocationID.xy + tileOffset.xy;

	if ( BoundsCheck( ivec2( location ) ) ) {
		// wang hash seeded uniquely for every pixel
		seed = wangSeed + 42069 * location.x + 451 * location.y;

		// subpixel offset, remap uv, etc
		const vec2 uv = ( vec2( location ) + SubpixelOffset() ) / vec2( imageSize( accumulatorColor ).xy );
		const vec2 uvRemapped = 2.0f * ( uv - vec2( 0.5f ) );

		ray r = getCameraRayForUV( uvRemapped );

		// this is a little bit redundant, need to maybe revisit at some point
		sceneIntersection s = GetNearestSceneIntersection( r.origin, r.direction );
		const float hitDistance = s.dTravel;
		const vec3 hitPoint = r.origin + hitDistance * r.direction;

		// existing values from the buffers
		const vec4 oldColor = imageLoad( accumulatorColor, ivec2( location ) );
		const vec4 oldNormalD = imageLoad( accumulatorNormalsAndDepth, ivec2( location ) );

		// increment the sample count
		const float sampleCount = oldColor.a + 1.0f;

		// new values - raymarch pathtrace
		const vec4 newColor = vec4( ColorSample( uvRemapped ), 1.0f );
		const vec4 newNormalD = vec4( s.normal, hitDistance );

		// blended with history based on sampleCount
		const float mixFactor = 1.0f / sampleCount;
		const vec4 mixedColor = vec4( mix( oldColor.rgb, newColor.rgb, mixFactor ), sampleCount );
		const vec4 mixedNormalD = vec4(
			clamp( mix( oldNormalD.xyz, newNormalD.xyz, mixFactor ), -1.0f, 1.0f ), // normals need to be -1..1
			mix( oldNormalD.w, newNormalD.w, mixFactor )
		);

		// store the values back
		imageStore( accumulatorColor, ivec2( location ), mixedColor );
		imageStore( accumulatorNormalsAndDepth, ivec2( location ), mixedNormalD );
	}
}
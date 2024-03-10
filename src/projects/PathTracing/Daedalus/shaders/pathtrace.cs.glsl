#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( location = 0, rgba8ui ) uniform uimage2D blueNoise;
layout( location = 1, rgba32f ) uniform image2D accumulatorColor;
layout( location = 2, rgba32f ) uniform image2D accumulatorNormalsAndDepth;

uniform ivec2 tileOffset;
uniform ivec2 noiseOffset;
uniform int wangSeed;

#include "random.h"		// rng + remapping utilities
#include "twigl.glsl"	// noise, some basic math utils
#include "hg_sdf.glsl"	// SDF modeling + booleans, etc
#include "mathUtils.h"	// couple random math utilities
#include "colorRamps.h" // 1d -> 3d color mappings

bool BoundsCheck( in ivec2 loc ) {
	const ivec2 b = ivec2( imageSize( accumulatorColor ) ).xy;
	return ( loc.x < b.x && loc.y < b.y && loc.x >= 0 && loc.y >= 0 );
}

vec4 Blue( in ivec2 loc ) {
	return imageLoad( blueNoise, ( loc + noiseOffset ) % imageSize( blueNoise ).xy );
}

//=============================================================================================================================
//=============================================================================================================================
#define NONE	0
#define BLUE	1
#define UNIFORM	2
#define WEYL	3
#define WEYLINT	4
//=============================================================================================================================
uniform int sampleNumber;
uniform int subpixelJitterMethod;
//=============================================================================================================================
vec2 SubpixelOffset () {
	vec2 offset = vec2( 0.0f );
	switch ( subpixelJitterMethod ) {
		case NONE:
			offset = vec2( 0.5f );
			break;

		case BLUE:
			bool oddFrame = ( sampleNumber % 2 == 0 );
			offset = ( oddFrame ?
				Blue( ivec2( gl_GlobalInvocationID.xy + tileOffset ) ).xy :
				Blue( ivec2( gl_GlobalInvocationID.xy + tileOffset ) ).zw ) / 255.0f;
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
//=============================================================================================================================
//=============================================================================================================================
uniform vec3 viewerPosition;
uniform vec3 basisX;
uniform vec3 basisY;
uniform vec3 basisZ;

uniform float uvScalar;
uniform int cameraType;
uniform int maxBounces;
uniform int bokehMode;
uniform float FoV;
uniform float maxDistance;
uniform float epsilon;
uniform float exposure;

uniform bool thinLensEnable;
uniform float thinLensFocusDistance;
uniform float thinLensJitterRadius;
//=============================================================================================================================
// key structs
//=============================================================================================================================
//=============================================================================================================================
struct ray_t {
	vec3 origin;
	vec3 direction;
};
//=============================================================================================================================
struct intersection_t {
	float dTravel;
	vec3 albedo;
	vec3 normal;
	bool frontfaceHit;
	int materialID;

	// tbd:
	// material properties... IoR, roughness...
	// absorption state...
};
intersection_t DefaultIntersection() {
	intersection_t intersection;
	intersection.dTravel = maxDistance;
	intersection.albedo = vec3( 0.0f );
	intersection.normal = vec3( 0.0f );
	intersection.frontfaceHit = false;
	intersection.materialID = 0;
	return intersection;
}
//=============================================================================================================================
struct result_t {
	vec4 color;
	vec4 normalD;
};
//=============================================================================================================================

//=============================================================================================================================
// getting the camera ray for the current pixel
//=============================================================================================================================
#define NORMAL		0
#define SPHERICAL	1
#define SPHERICAL2	2
#define SPHEREBUG	3
#define SIMPLEORTHO	4
#define ORTHO		5
#define COMPOUND	6
//=============================================================================================================================
ray_t GetCameraRayForUV( in vec2 uv ) { // switchable cameras ( fisheye, etc ) - Assumes -1..1 range on x and y
	const float aspectRatio = float( imageSize( accumulatorColor ).x ) / float( imageSize( accumulatorColor ).y );

	ray_t r;
	r.origin	= vec3( 0.0f );
	r.direction	= vec3( 0.0f );

	// apply uvScalar here, probably

	switch ( cameraType ) {
	case NORMAL: {
		r.origin = viewerPosition;
		r.direction = normalize( aspectRatio * uv.x * basisX + uv.y * basisY + ( 1.0f / FoV ) * basisZ );
		break;
	}

	case SPHERICAL: {
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

	case SPHERICAL2: {
		uv *= uvScalar;
		mat3 rotY = Rotate3D( uv.y, basisX ); // rotate up/down by uv.y
		mat3 rotX = Rotate3D( uv.x, basisY ); // rotate left/right by uv.x
		r.origin = viewerPosition;
		r.direction = rotX * rotY * basisZ;
		break;
	}

	case SPHEREBUG: {
		uv *= uvScalar;
		uv.y /= aspectRatio;
		uv = vec2( atan( uv.y, uv.x ) + 0.5f, ( length( uv ) + 0.5f ) * acos( -1.0f ) );
		vec3 baseVec = normalize( vec3( cos( uv.y ) * cos( uv.x ), sin( uv.y ), cos( uv.y ) * sin( uv.x ) ) );
		r.origin = viewerPosition;
		r.direction = normalize( -baseVec.x * basisX + baseVec.y * basisY + ( 1.0f / FoV ) * baseVec.z * basisZ );
		break;
	}

	case SIMPLEORTHO: { // basically a long focal length, not quite orthographic
	// this isn't correct - need to adjust ray origin with basisX and basisY, and set ray direction equal to basisZ
		uv.y /= aspectRatio;
		vec3 baseVec = vec3( uv * FoV, 1.0f );
		r.origin = viewerPosition;
		r.direction = normalize( basisX * baseVec.x + basisY * baseVec.y + basisZ * baseVec.z );
		break;
	}

	case ORTHO: {
		uv.y /= aspectRatio;
		r.origin = viewerPosition + basisX * FoV * uv.x + basisY * FoV * uv.y;
		r.direction = basisZ;
		break;
	}

	case COMPOUND: {
		// compound eye hex vision by Samon
		// https://www.shadertoy.com/view/4lfcR7

		// strange, original one wanted uv in 1..2
		// uv.y /= aspectRatio;
		uv.x *= aspectRatio;
		uv.xy /= 2.0f;
		uv.xy += 1.0f;

		// Estimate hex coordinate
		float R = 0.075f;
		vec2 grid;
		grid.y = floor( uv.y / ( 1.5f * R ) );
		float odd = mod( grid.y, 2.0f );
		grid.x = floor( uv.x / ( 1.732050807 * R ) - odd * 0.5f );

		// Find possible centers of hexagons
		vec2 id = grid;
		float o = odd;
		vec2 h1 = vec2( 1.732050807f * R * ( id.x + 0.5f * o ), 1.5f * id.y * R );
		id = grid + vec2( 1.0f, 0.0f ); o = odd;
		vec2 h2 = vec2( 1.732050807f * R * ( id.x + 0.5f * o ), 1.5f * id.y * R );
		id = grid + vec2( odd, 1.0f ); o = 1.0f - odd;
		vec2 h3 = vec2( 1.732050807f * R * ( id.x + 0.5f * o ), 1.5f * id.y * R );

		// Find closest center
		float d1 = dot( h1 - uv, h1 - uv );
		float d2 = dot( h2 - uv, h2 - uv );
		float d3 = dot( h3 - uv, h3 - uv );
		if ( d2 < d1 ) {
			d1 = d2;
			h1 = h2;
		}
		if ( d3 < d1 ) {
			d1 = d3;
			h1 = h3;
		}

		// Hexagon UV
		vec2 uv2 = uv - h1;

		// Set Hexagon offset
		const float offset = 2.0f;
		uv = ( uv.xy - 1.0f ) + uv2 * offset;

		// Per Facet Fisheye effect (optional)
		vec2 coords = ( uv2 - 0.5f * R ) * 2.0f;
		vec2 fisheye;
		float intensity = 1.0f;
		fisheye.x = ( 1.0f - coords.y * coords.y ) * intensity * ( coords.x );
		fisheye.y = ( 1.0f - coords.x * coords.x ) * intensity * ( coords.y );
		uv -= fisheye * R;

		// and now get the parameters from the remapped uv
		r.direction = normalize( aspectRatio * uv.x * basisX + uv.y * basisY + ( 1.0f / FoV ) * basisZ );
		r.origin = viewerPosition;
		break;
	}

	default:
		break;
	}

	if ( thinLensEnable || cameraType == SPHEREBUG ) { // or we want that fucked up split sphere behavior... sphericalFucked, something
		// thin lens adjustment
		vec3 focuspoint = r.origin + ( ( r.direction * thinLensFocusDistance ) / dot( r.direction, basisZ ) );
		vec2 diskOffset = thinLensJitterRadius * GetBokehOffset( bokehMode );
		r.origin += diskOffset.x * basisX + diskOffset.y * basisY;
		r.direction = normalize( focuspoint - r.origin );
	}

	return r;
}

//=============================================================================================================================
uniform sampler2D skyCache;
uniform bool skyInvert;
uniform float skyBrightnessScalar;
//=============================================================================================================================
vec3 SkyColor( ray_t ray ) {
	// sample the texture
	if ( skyInvert ) {
		ray.direction.y *= -1.0f;
	}
	vec2 samplePoint = vec2( 0.0f );
	float elevationFactor = dot( ray.direction, vec3( 0.0f, 1.0f, 0.0f ) );
	if ( abs( elevationFactor ) > 0.99f ) {
		// handle vertical samples, straight up or straight down - compress y into valid range
		elevationFactor = elevationFactor * 0.99f;
	}
	samplePoint.x = RangeRemapValue( atan( ray.direction.x, ray.direction.z ), -pi, pi, 0.0f, 1.0f );
	samplePoint.y = RangeRemapValue( elevationFactor, -1.0f, 1.0f, 0.0f, 1.0f );
	return texture( skyCache, samplePoint ).rgb * skyBrightnessScalar;
}

//=============================================================================================================================
// evaluate the material's effect on this ray
//=============================================================================================================================
#define NOHIT						0
#define EMISSIVE					1
#define DIFFUSE						2
#define DIFFUSEAO					3 // this never got used - besides, it only has AO for SDF geo
#define METALLIC					4
#define RAINBOW						5
#define MIRROR						6
#define PERFECTMIRROR				7
#define WOOD						8
#define MALACHITE					9
#define LUMARBLE					10
#define LUMARBLE2					11
#define LUMARBLE3					12
#define LUMARBLECHECKER				13
#define CHECKER						14
#define REFRACTIVE					15
#define REFRACTIVE_FROSTED			16
//=============================================================================================================================
// objects shouldn't have these materials set directly, it is used for backface hits
#define REFRACTIVE_BACKFACE			100
#define REFRACTIVE_FROSTED_BACKFACE	101
//=============================================================================================================================
bool EvaluateMaterial( inout vec3 finalColor, inout vec3 throughput, in intersection_t intersection, inout ray_t ray, in ray_t rayPrevious ) {
	// precalculate reflected vector, random diffuse vector, random specular vector
	const vec3 reflectedVector = reflect( rayPrevious.direction, intersection.normal );
	const vec3 randomVectorDiffuse = normalize( ( 1.0f + epsilon ) * intersection.normal + RandomUnitVector() );
	const vec3 randomVectorSpecular = normalize( ( 1.0f + epsilon ) * intersection.normal + mix( reflectedVector, RandomUnitVector(), 0.1f ) ); // refit this to handle roughness

	// if the ray escapes
	if ( intersection.dTravel >= maxDistance ) {

		// contribution from the skybox
		finalColor += throughput * SkyColor( ray );
		return false;

	} else {

		// material specific behavior
		switch( intersection.materialID ) {
		case NOHIT: // no op
			return false;
			break;

		case EMISSIVE: { // light emitting material
			// ray.direction = randomVectorDiffuse;
			finalColor += throughput * intersection.albedo;
			break;
		}

		case DIFFUSE: {
			throughput *= intersection.albedo;
			ray.direction = randomVectorDiffuse;
			break;
		}

		case METALLIC: {
			throughput *= intersection.albedo;
			ray.direction = randomVectorSpecular;
		}

		case MIRROR: {	// perfect mirror ( some attenuation )
			throughput *= 0.618f;
			ray.direction = reflectedVector;
			break;
		}

		default:
			break;
		}
	}
	// some material was encountered
	return true;
}

//=============================================================================================================================
// SDF RAYMARCH GEOMETRY
//=============================================================================================================================

// parameters
uniform bool raymarchEnable;
uniform float raymarchMaxDistance;
uniform float raymarchUnderstep;
uniform int raymarchMaxSteps;

// global state tracking
vec3 hitColor;
int hitSurfaceType;

//=============================================================================================================================

// float deFractal( vec3 p ) {
// 	float s=3.;
// 	for(int i = 0; i < 4; i++) {
// 		p=mod(p-1.,2.)-1.;
// 		float r=1.2/dot(p,p);
// 		p*=r; s*=r;
// 	}
// 	p = abs(p)-0.8;
// 	if (p.x < p.z) p.xz = p.zx;
// 	if (p.y < p.z) p.yz = p.zy;
// 	if (p.x < p.y) p.xy = p.yx;
// 	if (p.x < p.z) p.xz = p.zx;
// 	return length(cross(p,normalize(vec3(0,1,1))))/s-.001;
// }

float deFractal( vec3 pos ) {
	vec3 tpos = pos;
	tpos.xz = abs( 0.5f - mod( tpos.xz, 1.0f ) );
	vec4 p = vec4( tpos, 1.0f );
	float y = max( 0.0f, 0.35f - abs( pos.y - 3.35f ) ) / 0.35f;
	for ( int i = 0; i < 7; i++ ) {
		p.xyz = abs( p.xyz ) - vec3( -0.02f, 1.98f, -0.02f );
		p = p * ( 2.0f + 0.0f * y ) / clamp( dot( p.xyz, p.xyz ), 0.4f, 1.0f ) - vec4( 0.5f, 1.0f, 0.4f, 0.0f );
		p.xz *= mat2( -0.416f, -0.91f, 0.91f, -0.416f );
	}
	return ( length( max( abs( p.xyz ) - vec3( 0.1f, 5.0f, 0.1f ), vec3( 0.0f ) ) ) - 0.05f ) / p.w;
}

float deApollo( vec3 p0 ) {
	vec4 p = vec4( p0, 1.0f );
	for ( int i = 0; i < 8; i++ ) {
		p.xyz = mod( p.xyz - 1.0f, 2.0f ) - 1.0f;
		p *= 1.4f / dot( p.xyz, p.xyz );
	}
	return ( length( p.xz / p.w ) * 0.25f );
}

float deLeaf( vec3 p ) {
	float S = 1.0f;
	float R, e;
	float time = 0.5f;
	p.y += p.z;
	p = vec3( log( R = length( p ) ) - time, asin( -p.z / R ), atan( p.x, p.y ) + time );
	for( e = p.y - 1.5f; S < 6e2; S += S )
		e += sqrt( abs( dot( sin( p.zxy * S ), cos( p * S ) ) ) ) / S;
	return e * R * 0.1f;
}

float deStairs( vec3 P ) {
	vec3 Q;
	float a, d = min( ( P.y - abs( fract( P.z ) - 0.5f ) ) * 0.7f, 1.5f - abs( P.x ) );
	for ( a = 2.0f; a < 6e2f; a += a )
		Q = P * a,
		Q.xz *= rotate2D( a ),
		d += abs( dot( sin( Q ), Q - Q + 1.0f ) ) / a / 7.0f;
	return d;
}

float deAnemone( vec3 p ) {
	#define V vec2(0.7f,-0.7f)
	#define G(p)dot(p,V)
	float i = 0.0f, g = 0.0f, e = 1.0f;
	float t = 0.34f; // change to see different behavior
	for ( int j = 0; j++ < 8; ) {
		p = abs( rotate3D( 0.34f, vec3( 1.0f, -3.0f, 5.0f ) ) * p * 2.0f ) - 1.0f,
		p.xz -= ( G( p.xz ) - sqrt( G( p.xz ) * G( p.xz ) + 0.05f ) ) * V;
	}
	return length( p.xz ) / 3e2f;
}

float dePillars( vec3 pos ) {
	vec3 tpos=pos;
	tpos.xz=abs(.5-mod(tpos.xz,1.));
	vec4 p=vec4(tpos,1.);
	float y=max(0.,.35-abs(pos.y-3.35))/.35;
	for (int i=0; i<7; i++) {
		p.xyz = abs(p.xyz)-vec3(-0.02,1.98,-0.02);
		p=p*(2.0+0.*y)/clamp(dot(p.xyz,p.xyz),.4,1.)-vec4(0.5,1.,0.4,0.);
		p.xz*=mat2(-0.416,-0.91,0.91,-0.416);
	}
	return (length(max(abs(p.xyz)-vec3(0.1,5.0,0.1),vec3(0.0)))-0.05)/p.w;
}

float deWater( vec3 p ) {
  float e, i=0., j, f, a, w;
 // p.yz *= mat2(cos(0.7f),sin(0.7f),-sin(0.7f),cos(0.7f));
  f = 0.3; // wave amplitude
  i < 45. ? p : p -= .001;
  e = p.y + 5.;
  for( a = j = .9; j++ < 30.; a *= .8 ){
    vec2 m = vec2( 1. ) * mat2(cos(j),sin(j),-sin(j),cos(j));
    // float x = dot( p.xz, m ) * f + t + t; // time varying behavior
    float x = dot( p.xz, m ) * f + 0.;
    w = exp( sin( x ) - 1. );
    p.xz -= m * w * cos( x ) * a;
    e -= w * a;
    f *= 1.2;
  }
  return e;
}

//=============================================================================================================================

float de( in vec3 p ) {
	vec3 pOriginal = p;
	float sceneDist = 1000.0f;
	hitColor = vec3( 0.0f );
	hitSurfaceType = NOHIT;

	// form for the following:
		// const d = blah whatever SDF
		// sceneDist = min( sceneDist, d )
		// if ( sceneDist == d && d < epsilon ) {
			// set material specifics - hitColor, hitSurfaceType
		// }

	// const float dFractal = max( deLeaf( p ), fRoundedBox( p, vec3( 3.0f, 3.0f, 5.0f ), 0.1f ) );
	// const float dFractal = deLeaf( p );
	const float dFractal = deWater( p );
	sceneDist = min( sceneDist, dFractal );
	if ( sceneDist == dFractal && dFractal < epsilon ) {
		hitColor = vec3( 0.1618f );
		// hitSurfaceType = NormalizedRandomFloat() < 0.1f ? MIRROR : DIFFUSE;
		hitSurfaceType = METALLIC;
	}

	const float dBar = fRoundedBox( p + vec3( 0.0f, 1.0f, 0.0f ), vec3( 0.15f, 0.15f, 100.0f ), 0.03f );
	sceneDist = min( sceneDist, dBar );
	if ( sceneDist == dBar && dBar < epsilon ) {
		// hitColor = vec3( 0.0f, 0.0f, 0.618f ) * 4.0f;
		hitColor = refPalette( smoothstep( saturate( RangeRemapValue( p.z, -10.0f, 10.0f, 0.0f, 1.0f ) ), -1.0f, 0.0f ), 10 ).xyz * 1.0f;
		hitSurfaceType = EMISSIVE;
	}

	return sceneDist;
}

//=============================================================================================================================

vec3 SDFNormal( in vec3 position ) {
	vec2 e = vec2( epsilon, 0.0f );
	return normalize( vec3( de( position ) ) - vec3( de( position - e.xyy ), de( position - e.yxy ), de( position - e.yyx ) ) );
}

//=============================================================================================================================

intersection_t raymarch( in ray_t ray ) {
	float dQuery = 0.0f;
	float dTotal = 0.0f;
	vec3 pQuery = ray.origin;
	for ( int steps = 0; steps < raymarchMaxSteps; steps++ ) {
		pQuery = ray.origin + dTotal * ray.direction;
		dQuery = de( pQuery );
		dTotal += dQuery * raymarchUnderstep;
		if ( dTotal > raymarchMaxDistance || abs( dQuery ) < epsilon ) {
			break;
		}
	}

	// fill out the intersection struct
	intersection_t intersection;
	intersection.dTravel = dTotal;
	intersection.albedo = hitColor;
	intersection.materialID = hitSurfaceType;
	intersection.normal = SDFNormal( ray.origin + dTotal * ray.direction );
	intersection.frontfaceHit = ( dot( intersection.normal, ray.direction ) <= 0.0f );
	return intersection;
}

//=============================================================================================================================

// refactor this, it's a bunch of ugly shit

// explicit intersection primitives
struct iqIntersect {
	vec4 a;  // distance and normal at entry
	vec4 b;  // distance and normal at exit
};

// false Intersection
const iqIntersect kEmpty = iqIntersect(
	vec4( 1e20f, 0.0f, 0.0f, 0.0f ),
	vec4( -1e20f, 0.0f, 0.0f, 0.0f )
);

bool IsEmpty( iqIntersect i ) {
	return i.b.x < i.a.x;
}

iqIntersect IntersectSphere ( in ray_t ray, in vec3 center, float radius ) {
	// https://iquilezles.org/articles/intersectors/
	vec3 oc = ray.origin - center;
	float b = dot( oc, ray.direction );
	float c = dot( oc, oc ) - radius * radius;
	float h = b * b - c;
	if ( h < 0.0f ) return kEmpty; // no intersection
	h = sqrt( h );
	// h is known to be positive at this point, b+h > b-h
	float nearHit = -b - h; vec3 nearNormal = normalize( ( ray.origin + ray.direction * nearHit ) - center );
	float farHit  = -b + h; vec3 farNormal  = normalize( ( ray.origin + ray.direction * farHit ) - center );
	return iqIntersect( vec4( nearHit, nearNormal ), vec4( farHit, farNormal ) );
}

iqIntersect IntersectBox( in ray_t ray, in vec3 center, in vec3 size ) {
	vec3 oc = ray.origin - center;
	vec3 m = 1.0f / ray.direction;
	vec3 k = vec3( ray.direction.x >= 0.0f ? size.x : -size.x, ray.direction.y >= 0.0f ? size.y : -size.y, ray.direction.z >= 0.0f ? size.z : -size.z );
	vec3 t1 = ( -oc - k ) * m;
	vec3 t2 = ( -oc + k ) * m;
	float tN = max( max( t1.x, t1.y ), t1.z );
	float tF = min( min( t2.x, t2.y ), t2.z );
	if ( tN > tF || tF < 0.0f ) return kEmpty;
	return iqIntersect(
		vec4( tN, normalize( -sign( ray.direction ) * step( vec3( tN ), t1 ) ) ),
		vec4( tF, normalize( -sign( ray.direction ) * step( t2, vec3( tF ) ) ) ) );
}

//=============================================================================================================================

uniform bool ddaSpheresEnable;
uniform float ddaSpheresBoundSize;
uniform int ddaSpheresResolution;

bool CheckValidityOfIdx( ivec3 idx ) {
	// return true;

	// return snoise3D( idx * 0.01f ) < 0.0f;

	bool mask = deStairs( idx * 0.01f - vec3( 2.9f ) ) < 0.001f;
	// return deApollo( idx * 0.01f - vec3( 0.5f ) ) < 0.01f;

	// bool blackOrWhite = ( step( -0.05f,
	// 	cos( PI * 0.01f * float( idx.x ) + PI / 2.0f ) *
	// 	cos( PI * 0.01f * float( idx.y ) + PI / 2.0f ) *
	// 	cos( PI * 0.01f * float( idx.z ) + PI / 2.0f ) ) == 0 );

	// return blackOrWhite && mask;
	return mask;
}

vec3 GetOffsetForIdx( ivec3 idx ) {
	// return vec3( 0.5f );

	// jitter... not liking the look of this, much
	return vec3( 0.26f ) + 0.45f * ( pcg3d( uvec3( idx ) ) / 4294967296.0f );
}

float GetRadiusForIdx( ivec3 idx ) {
	// return 0.25f;

	return 0.45f * ( pcg3d( uvec3( idx ) ) / 4294967296.0f ).x;

	// return saturate( ( snoise3D( idx * 0.01f ) / 2.0f ) + 1.0f ) / 2.0f;
	// return saturate( snoise3D( idx * 0.04f ) / 4.0f );
}

vec3 GetColorForIdx( ivec3 idx ) {
	// return vec3( 0.9f );
	// return vec3( 0.9f, 0.5f, 0.2f ) * ( pcg3d( uvec3( idx ) ) / 4294967296.0f );
	return mix( vec3( 0.618f ), vec3( 0.618f, 0.80f, 0.0f ), saturate( pcg3d( uvec3( idx ) ) / 4294967296.0f ) );
}

int GetMaterialIDForIdx( ivec3 idx ) {
	// return DIFFUSE;
	// return MIRROR;
	return NormalizedRandomFloat() < 0.9f ? MIRROR : DIFFUSE;
}

intersection_t DDATraversal( in ray_t ray, in float distanceToBounds ) {
	ray_t rayCache = ray;

	// accounting for the bounding box
	if ( distanceToBounds > 0.0f ) {
		ray.origin += ray.direction * distanceToBounds;
	}

	// map the ray into the integer grid space
	const float res = float( ddaSpheresResolution );
	const float epsilon = 0.001f;
	ray.origin = vec3(
		RangeRemapValue( ray.origin.x, -ddaSpheresBoundSize / 2.0f, ddaSpheresBoundSize / 2.0f, epsilon, res - epsilon ),
		RangeRemapValue( ray.origin.y, -ddaSpheresBoundSize / 2.0f, ddaSpheresBoundSize / 2.0f, epsilon, res - epsilon ),
		RangeRemapValue( ray.origin.z, -ddaSpheresBoundSize / 2.0f, ddaSpheresBoundSize / 2.0f, epsilon, res - epsilon )
	);

	// prep for traversal
	bool hitFound = false;
	intersection_t intersection = DefaultIntersection();

	// DDA traversal
	// from https://www.shadertoy.com/view/7sdSzH
	vec3 deltaDist = 1.0f / abs( ray.direction );
	ivec3 rayStep = ivec3( sign( ray.direction ) );
	bvec3 mask0 = bvec3( false );
	ivec3 mapPos0 = ivec3( floor( ray.origin ) );
	vec3 sideDist0 = ( sign( ray.direction ) * ( vec3( mapPos0 ) - ray.origin ) + ( sign( ray.direction ) * 0.5f ) + 0.5f ) * deltaDist;

	#define MAX_RAY_STEPS 2000
	for ( int i = 0; i < MAX_RAY_STEPS && ( all( greaterThanEqual( mapPos0, ivec3( 0 ) ) ) && all( lessThan( mapPos0, ivec3( ddaSpheresResolution ) ) ) ); i++ ) {
	// for ( int i = 0; i < MAX_RAY_STEPS; i++ ) {

		// Core of https://www.shadertoy.com/view/4dX3zl Branchless Voxel Raycasting
		bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
		vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
		ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

		// checking mapPos0 for hit
		if ( CheckValidityOfIdx( mapPos0 ) ) {

			// see if we found an intersection - ball will almost fill the grid cell
			iqIntersect test = IntersectSphere( ray, vec3( mapPos0 ) + GetOffsetForIdx( mapPos0 ), GetRadiusForIdx( mapPos0 ) );
			// iqIntersect test = IntersectBox( ray, vec3( mapPos0 ) + vec3( 0.5f ), vec3( 0.5f ) );
			const bool behindOrigin = ( test.a.x < 0.0f && test.b.x < 0.0f );

			// update ray, to indicate hit location
			if ( !IsEmpty( test ) && !behindOrigin ) {
				intersection.frontfaceHit = !( test.a.x < 0.0f && test.b.x >= 0.0f );

				// get the location of the intersection
				ray.origin = ray.origin + ray.direction * ( intersection.frontfaceHit ? test.a.x : test.b.x );

				// map the ray back into the world space
				ray.origin = vec3(
					RangeRemapValue( ray.origin.x, epsilon, res - epsilon, -ddaSpheresBoundSize / 2.0f, ddaSpheresBoundSize / 2.0f ),
					RangeRemapValue( ray.origin.y, epsilon, res - epsilon, -ddaSpheresBoundSize / 2.0f, ddaSpheresBoundSize / 2.0f ),
					RangeRemapValue( ray.origin.z, epsilon, res - epsilon, -ddaSpheresBoundSize / 2.0f, ddaSpheresBoundSize / 2.0f )
				);

				intersection.dTravel = distance( ray.origin, rayCache.origin );
				intersection.normal = intersection.frontfaceHit ? test.a.yzw : test.b.yzw;
				intersection.materialID = GetMaterialIDForIdx( mapPos0 );
				intersection.albedo = GetColorForIdx( mapPos0 );
				break;
			}
		}

		sideDist0 = sideDist1;
		mapPos0 = mapPos1;
	}

	// return the state, when we drop out of the loop
	return intersection;
}


intersection_t VoxelIntersection( in ray_t ray ) {
	intersection_t intersection = DefaultIntersection();

	iqIntersect boundingBoxIntersection = IntersectBox( ray, vec3( 0.0f ), vec3( ddaSpheresBoundSize / 2.0f ) );
	const bool boxBehindOrigin = ( boundingBoxIntersection.a.x < 0.0f && boundingBoxIntersection.b.x < 0.0f );
	const bool backfaceHit = ( boundingBoxIntersection.a.x < 0.0f && boundingBoxIntersection.b.x >= 0.0f );

	if ( !IsEmpty( boundingBoxIntersection ) && !boxBehindOrigin ) {
		intersection = DDATraversal( ray, boundingBoxIntersection.a.x );
	}

	return intersection;
}

//=============================================================================================================================

intersection_t GetNearestSceneIntersection( in ray_t ray ) {
	// return a single intersection_t representing the closest ray intersection
	intersection_t SDFResult	= raymarchEnable ? raymarch( ray ) : DefaultIntersection();
	intersection_t VoxelResult	= ddaSpheresEnable ? VoxelIntersection( ray ) : DefaultIntersection();

	intersection_t result = DefaultIntersection();
	float minDistance = vmin( vec2( SDFResult.dTravel, VoxelResult.dTravel ) );
	if ( minDistance == SDFResult.dTravel ) {
		result = SDFResult;
	} else {
		result = VoxelResult;
	}

	return result;
}

//=============================================================================================================================
//=============================================================================================================================

result_t GetNewSample( in vec2 uv ) {
	// get the camera ray for this uv
	const ray_t rayInitial = GetCameraRayForUV( uv );

	// initialize state for the ray
	result_t result;

	// initialize the "previous", "current" values
	ray_t ray, rayPrevious;
	ray = rayPrevious = rayInitial;

	// pathtracing accumulators
	vec3 finalColor = vec3( 0.0f );
	vec3 throughput = vec3( 1.0f );

	for ( int bounce = 0; bounce < maxBounces; bounce++ ) {
		// get the scene intersection for this bounce
		intersection_t intersection = GetNearestSceneIntersection( ray );

		if ( bounce == 0 ) // first hit populates the normal/depth buffer result
			result.normalD = vec4( intersection.normal, intersection.dTravel );

		// update the ray values
		rayPrevious = ray;
		ray.origin = rayPrevious.origin + intersection.dTravel * rayPrevious.direction;

		// epsilon bump, along the normal vector, for non-refractive surfaces
		if ( intersection.materialID != REFRACTIVE && intersection.materialID != REFRACTIVE_BACKFACE )
			ray.origin += 2.0f * epsilon * intersection.normal;

		// evaluate the material - updates finalColor, throughput, ray - return false on ray escape
		if ( !EvaluateMaterial( finalColor, throughput, intersection, ray, rayPrevious ) ) break;

		// russian roulette termination
		float maxChannel = vmax( throughput );
		if ( NormalizedRandomFloat() > maxChannel ) break;
		throughput *= 1.0f / maxChannel; // compensation term
	}

	// apply final exposure value and return
	result.color.rgb = finalColor * exposure;
	return result;
}

//=============================================================================================================================
//=============================================================================================================================

void ProcessTileInvocation( in ivec2 pixel ) {
	// calculate the UV from the pixel location
	vec2 uv = ( vec2( pixel ) + SubpixelOffset() ) / imageSize( accumulatorColor ).xy;
	uv = uv * 2.0f - vec2( 1.0f );

	// get the new color sample
	result_t newData = GetNewSample( uv );

	// load the previous color, normal, depth values
	vec4 previousColor = imageLoad( accumulatorColor, pixel );
	vec4 previousNormalD = imageLoad( accumulatorNormalsAndDepth, pixel );

	// mix the new and old values based on the current sampleCount
	float sampleCount = previousColor.a + 1.0f;
	const float mixFactor = 1.0f / sampleCount;
	const vec4 mixedColor = vec4( mix( previousColor.rgb, newData.color.rgb, mixFactor ), sampleCount );
	const vec4 mixedNormalD = vec4(
		clamp( mix( previousNormalD.xyz, newData.normalD.xyz, mixFactor ), -1.0f, 1.0f ),
		mix( previousNormalD.w, newData.normalD.w, mixFactor )
	);

	// store the values back
	imageStore( accumulatorColor, pixel, mixedColor );
	imageStore( accumulatorNormalsAndDepth, pixel, mixedNormalD );
}

//=============================================================================================================================
//=============================================================================================================================

void main() {
	const ivec2 location = ivec2( gl_GlobalInvocationID.xy ) + tileOffset.xy;
	if ( BoundsCheck( location ) ) {
		// seed defined in random.h, value used for the wang hash
		seed = wangSeed + 42069 * location.x + 451 * location.y;
		ProcessTileInvocation( location );
	}
}
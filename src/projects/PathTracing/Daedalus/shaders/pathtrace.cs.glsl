#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
//=============================================================================================================================
layout( location = 0, rgba8ui ) readonly uniform uimage2D blueNoise;
layout( location = 1, rgba32f ) uniform image2D accumulatorColor;
layout( location = 2, rgba32f ) uniform image2D accumulatorNormalsAndDepth;
//=============================================================================================================================
uniform ivec2 tileOffset;
uniform ivec2 noiseOffset;
uniform int wangSeed;
//=============================================================================================================================
#include "random.h"		// rng + remapping utilities
#include "twigl.glsl"	// noise, some basic math utils
#include "noise.h"		// more noise
#include "wood.h"		// DeanTheCoder's procedural wood texture
#include "hg_sdf.glsl"	// SDF modeling + booleans, etc
#include "mathUtils.h"	// couple random math utilities
#include "colorRamps.glsl.h" // 1d -> 3d color mappings
#include "glyphs.h"		// the uint encoded glyph masks
//=============================================================================================================================
bool BoundsCheck( in ivec2 loc ) {
	const ivec2 b = ivec2( imageSize( accumulatorColor ) ).xy;
	return ( loc.x < b.x && loc.y < b.y && loc.x >= 0 && loc.y >= 0 );
}
//=============================================================================================================================
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
uniform bool cameraOriginJitter;
uniform float voraldoCameraScalar;
uniform int maxBounces;
uniform int bokehMode;
uniform float FoV;
uniform float maxDistance;
uniform float epsilon;
uniform float exposure;

uniform float marbleRadius;

uniform bool thinLensEnable;
uniform float thinLensFocusDistance;
uniform float thinLensJitterRadiusInner;
uniform float thinLensJitterRadiusOuter;
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
	float IoR;
	float roughness;

	// tbd:
	// material properties... IoR, roughness...
	// absorption state...
};
//=============================================================================================================================
// default "constructor"
//=============================================================================================================================
intersection_t DefaultIntersection() {
	intersection_t intersection;
	intersection.dTravel = maxDistance;
	intersection.albedo = vec3( 0.0f );
	intersection.normal = vec3( 0.0f );
	intersection.frontfaceHit = false;
	intersection.materialID = 0;
	intersection.IoR = 1.0f / 1.5f;
	intersection.roughness = 0.1f;
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
#define VORALDO		7
//=============================================================================================================================
ray_t GetCameraRayForUV( in vec2 uv ) { // switchable cameras ( fisheye, etc ) - Assumes -1..1 range on x and y
	const float aspectRatio = float( imageSize( accumulatorColor ).x ) / float( imageSize( accumulatorColor ).y );

	const vec2 uvCache = uv;

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
		baseVec = Rotate3D( PI / 2.0f, vec3( 2.5f, 0.4f, 1.0f ) ) * baseVec; // this is to match the other camera
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

	case VORALDO:
	{
		uv.y /= aspectRatio;
		vec3 viewerOffset = basisX * FoV * uv.x + basisY * FoV * uv.y;
		r.origin = viewerPosition + viewerOffset;
		r.direction = normalize( 2.0f * basisZ + viewerOffset * voraldoCameraScalar );

		break;
	}

	default:
		break;
	}

	if ( cameraOriginJitter ) {
		r.origin += ( Blue( ivec2( gl_GlobalInvocationID.xy ) ).z / 255.0f ) * 0.1f * r.direction;
	}

	if ( thinLensEnable || cameraType == SPHEREBUG ) { // or we want that fucked up split sphere behavior... sphericalFucked, something
		// thin lens adjustment
		vec3 focuspoint = r.origin + ( ( r.direction * thinLensFocusDistance ) / dot( r.direction, basisZ ) );

		// maybe try some different mappings... something non-linear, ramping up more quickly at the edges
		const float dCenter = distance( uvCache, vec2( 0.0f ) );
		float lerpedRadius = mix( thinLensJitterRadiusInner, thinLensJitterRadiusOuter, dCenter * dCenter * dCenter );
		vec2 diskOffset = lerpedRadius * GetBokehOffset( bokehMode );
		r.origin += diskOffset.x * basisX + diskOffset.y * basisY;
		r.direction = normalize( focuspoint - r.origin );
	}

	return r;
}

//=============================================================================================================================
uniform sampler2D skyCache;
uniform bool skyInvert;
uniform float skyBrightnessScalar;
uniform float skyClamp;
//=============================================================================================================================
vec3 SkyColor( ray_t ray ) {
	// sample the texture
	if ( skyInvert ) {
		ray.direction.y *= -1.0f;
	}

	float elevationFactor = dot( ray.direction, vec3( 0.0f, 1.0f, 0.0f ) );
	const float epsilonBump = 1.0f / 4096.0f;
	vec2 samplePoint = vec2(
		RangeRemapValue( atan( ray.direction.x, ray.direction.z ), -pi, pi, 0.0f + epsilonBump / 2.0f, 1.0f - epsilonBump / 2.0f ),
		// RangeRemapValue( atan( ray.direction.x, ray.direction.z ), -pi, pi, 0.0f, 1.0f ),
		RangeRemapValue( elevationFactor, -1.0f, 1.0f, 0.0f, 1.0f )
	);

	if ( skyClamp > 0.0f ) {
		return clamp( texture( skyCache, samplePoint ).rgb * skyBrightnessScalar, vec3( 0.0f ), vec3( skyClamp ) );
	} else {
		return texture( skyCache, samplePoint ).rgb * skyBrightnessScalar;
	}
}
//=============================================================================================================================
// evaluate the material's effect on this ray
//=============================================================================================================================
#define NOHIT						0
#define EMISSIVE					1
#define DIFFUSE						2
#define METALLIC					3
#define MIRROR						4
#define PERFECTMIRROR				5
#define POLISHEDWOOD				6
#define WOOD						7
#define MALACHITE					8
#define LUMARBLE					9
#define LUMARBLE2					10
#define LUMARBLE3					11
#define LUMARBLECHECKER				12
#define CHECKER						13
#define REFRACTIVE					14
//=============================================================================================================================
// objects shouldn't have this material set directly, it is used for backface hits
#define REFRACTIVE_BACKFACE			100
//=============================================================================================================================
float Reflectance ( const float cosTheta, const float IoR ) {
	// Use Schlick's approximation for reflectance
	float r0 = ( 1.0f - IoR ) / ( 1.0f + IoR );
	r0 = r0 * r0;
	return r0 + ( 1.0f - r0 ) * pow( ( 1.0f - cosTheta ), 5.0f );
}
//=============================================================================================================================
bool EvaluateMaterial( inout vec3 finalColor, inout vec3 throughput, in intersection_t intersection, inout ray_t ray, in ray_t rayPrevious ) {
	// precalculate reflected vector, random diffuse vector, random specular vector
	const vec3 reflectedVector = reflect( rayPrevious.direction, intersection.normal );
	const vec3 randomVectorCosineWeighted = cosWeightedRandomHemisphereDirection( intersection.normal );
	const vec3 randomVectorDiffuse = normalize( ( 1.0f + epsilon ) * intersection.normal + RandomUnitVector() );
	const vec3 randomVectorSpecular = normalize( ( 1.0f + epsilon ) * intersection.normal + mix( reflectedVector, RandomUnitVector(), intersection.roughness ) );

	// handle volumetrics, here? something where you attenuate by a term, weighted by exp( -distance )
		// with a chance to randomly scatter, which will give a random ray, and the position will be the point where it scatters

// // this sucks
// 	if ( ( NormalizedRandomFloat() * maxDistance ) < intersection.dTravel ) {
// 		throughput *= exp( -intersection.dTravel * 0.0003f );
// 		ray.direction = normalize( rayPrevious.direction + 0.125f * RandomUnitVector() );
// 		ray.origin = NormalizedRandomFloat() * intersection.dTravel * rayPrevious.direction + rayPrevious.origin;
// 		return true;
// 	}

	// if the ray escapes
	if ( intersection.dTravel >= maxDistance ) {

		// contribution from the skybox - dithered slightly to deal with banding
		finalColor += throughput * SkyColor( ray ) + ( RandomUnitVector() * 0.02f - 0.01f );
		return false;

	} else {

		// material specific behavior
		switch( intersection.materialID ) {
		case NOHIT: { // no op
			return false;
			break;
		}

		case EMISSIVE: { // light emitting material
			ray.direction = randomVectorDiffuse;
			finalColor += throughput * intersection.albedo;
			break;
		}

		case DIFFUSE: {
			throughput *= intersection.albedo;
			// ray.direction = randomVectorDiffuse;
			ray.direction = randomVectorCosineWeighted;
			break;
		}

		case METALLIC: {
			throughput *= intersection.albedo;
			ray.direction = randomVectorSpecular;
		}

		case MIRROR: {	// mirror surface ( some attenuation )
			// throughput *= 0.618f;
			throughput *= intersection.albedo;
			ray.direction = reflectedVector;
			break;
		}

		case PERFECTMIRROR: {	// perfect mirror
			ray.direction = reflectedVector;
			break;
		}

		case POLISHEDWOOD: {
			if ( NormalizedRandomFloat() < 0.01f ) { // mirror behavior
				ray.direction = reflectedVector;
			} else {
				throughput *= matWood( ray.origin / 10.0f );
				ray.direction = randomVectorDiffuse;
			}
			break;
		}

		case WOOD: {
			throughput *= matWood( ray.origin / 10.0f );
			ray.direction = randomVectorDiffuse;
			break;
		}

		case MALACHITE: {
			if ( NormalizedRandomFloat() < 0.1f ) {
				ray.direction = reflectedVector;
			} else {
				throughput *= matWood( ray.origin ).brg; // this swizzle makes a nice light green / dark green mix
				ray.direction = randomVectorDiffuse;
			}
			break;
		}

		case LUMARBLE: {
			if ( NormalizedRandomFloat() < 0.05f ) {
				ray.direction = reflectedVector;
			} else {
				throughput *= GetLuma( matWood( ray.origin ) ); // grayscale version of the wood material
				ray.direction = randomVectorDiffuse;
			}
			break;
		}

		case LUMARBLE2: {
			float lumaValue = GetLuma( matWood( ray.origin ) ).x;
			throughput *= lumaValue;
			ray.direction = normalize( ( 1.0f + epsilon ) * intersection.normal + mix( reflectedVector, RandomUnitVector(), 1.0f - lumaValue ) );
			break;
		}

		case LUMARBLE3: {
			float lumaValue = GetLuma( matWood( ray.origin ) ).x;
			throughput *= lumaValue;
			ray.direction = normalize( ( 1.0f + epsilon ) * intersection.normal + mix( reflectedVector, RandomUnitVector(), lumaValue ) );
			break;
		}

		case LUMARBLECHECKER: {
			// the checkerboard
			bool blackOrWhite = ( step( 0.0f,
				cos( PI * ray.origin.x + PI / 2.0f ) *
				cos( PI * ray.origin.y + PI / 2.0f ) *
				cos( PI * ray.origin.z + PI / 2.0f ) ) == 0 );

			float lumaValue = GetLuma( matWood( ray.origin ) ).x;
			throughput *= lumaValue;
			ray.direction = normalize( ( 1.0f + epsilon ) * intersection.normal + mix( reflectedVector, RandomUnitVector(), blackOrWhite ? lumaValue : 1.0f - lumaValue ) );
			break;
		}

		case CHECKER: {
			// diffuse material
			const float thresh = 0.01f;
			const float threshSmaller = 0.005f;
			vec3 p = ray.origin;

			bool redLines = (
				// thick lines at zero
				( abs( p.x ) < thresh ) ||
				( abs( p.y ) < thresh ) ||
				( abs( p.z ) < thresh ) ||
				// thinner lines every 5
				( abs( mod( p.x, 5.0f ) ) < threshSmaller ) ||
				( abs( mod( p.y, 5.0f ) ) < threshSmaller ) ||
				( abs( mod( p.z, 5.0f ) ) < threshSmaller ) );

			// the checkerboard
			bool blackOrWhite = ( step( 0.0f,
				cos( PI * p.x + PI / 2.0f ) *
				cos( PI * p.y + PI / 2.0f ) *
				cos( PI * p.z + PI / 2.0f ) ) == 0 );

			vec3 color = redLines ? vec3( 1.0f, 0.1f, 0.1f ) : ( blackOrWhite ? vec3( 0.618f ) : vec3( 0.1618f ) );
			throughput *= color;
			ray.direction = ( blackOrWhite || redLines ) ? randomVectorDiffuse : reflectedVector;
			break;
		}

		case REFRACTIVE: {
			throughput *= intersection.albedo; // temp

			ray.origin -= 2.0f * epsilon * intersection.normal;
			float cosTheta = min( dot( -normalize( ray.direction ), intersection.normal ), 1.0f );
			float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
			bool cannotRefract = ( intersection.IoR * sinTheta ) > 1.0f; // accounting for TIR effects
			if ( cannotRefract || Reflectance( cosTheta, intersection.IoR ) > NormalizedRandomFloat() ) {
				ray.direction = normalize( mix( reflect( normalize( ray.direction ), intersection.normal ), RandomUnitVector(), intersection.roughness ) );
			} else {
				ray.direction = normalize( mix( refract( normalize( ray.direction ), intersection.normal, intersection.IoR ), RandomUnitVector(), intersection.roughness ) );
			}
			break;
		}

		case REFRACTIVE_BACKFACE: {
			throughput *= intersection.albedo; // temp

			ray.origin += 2.0f * epsilon * intersection.normal;
			intersection.normal = -intersection.normal;
			float adjustedIOR = 1.0f / intersection.IoR;
			float cosTheta = min( dot( -normalize( ray.direction ), intersection.normal ), 1.0f );
			float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );
			bool cannotRefract = ( adjustedIOR * sinTheta ) > 1.0f; // accounting for TIR effects
			if ( cannotRefract || Reflectance( cosTheta, adjustedIOR ) > NormalizedRandomFloat() ) {
				ray.direction = normalize( mix( reflect( normalize( ray.direction ), intersection.normal ), RandomUnitVector(), intersection.roughness ) );
			} else {
				ray.direction = normalize( mix( refract( normalize( ray.direction ), intersection.normal, adjustedIOR ), RandomUnitVector(), intersection.roughness ) );
			}
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
uniform bool raymarchEnable;
uniform float raymarchMaxDistance;
uniform float raymarchUnderstep;
uniform int raymarchMaxSteps;
//=============================================================================================================================
// global state tracking
vec3 hitColor;
int hitSurfaceType;
float hitRoughness;
vec3 orbitColor = vec3( 0.0f );
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

float deFractal2 ( vec3 p ) {
  float d = 0.0f;
  float t = 0.3f;
  float s = 1.0f;
  vec4 r = vec4( 0.0f );
  vec4 q = vec4( p, 0.0f );
  for ( int j = 0; j < 4 ; j++ )
    r = max( r *= r *= r = mod( q * s + 1.0f, 2.0f ) - 1.0f, r.yzxw ),
    d = max( d, ( 0.27f - length( r ) * 0.3f ) / s ),
    s *= 3.1f;
  return d;
}

#define rot(a) mat2(cos(a),sin(a),-sin(a),cos(a))
float deJune ( vec3 p ) {
	p=abs(p)-3.;
	if(p.x < p.z)p.xz=p.zx;
	if(p.y < p.z)p.yz=p.zy;
	if(p.x < p.y)p.xy=p.yx;
	float s=2.; vec3 off=p*.5;
	for(int i=0;i<12;i++){
		p=1.-abs(p-1.);
		float k=-1.1*max(1.5/dot(p,p),1.5);
		s*=abs(k); p*=k; p+=off;
		p.zx*=rot(-1.2);
	}
	float a=2.5;
	p-=clamp(p,-a,a);
	return length(p)/s;
}

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

float deLeaf( vec3 p ) {
	float S = 1.0f;
	float R, e;
	float time = 0.5f;
	p.y += p.z;
	p = vec3( log( R = length( p ) ) - time, asin( -p.z / R ), atan( p.x, p.y ) + time );
	for( e = p.y - 1.5f; S < 6e2; S += S ) {
		e += sqrt( abs( dot( sin( p.zxy * S ), cos( p * S ) ) ) ) / S;
		orbitColor = mix( orbitColor, refPalette( mod( 0.4f * S, 1.0f ), INFERNO2 ).xyz, 0.9f );
	}
	return e * R * 0.1f;
}

float deGazTemple1( vec3 p ){
	float s = 2.;
	float e = 0.;
	for(int j=0;++j<7;)
		p.xz=abs(p.xz)-2.3,
		p.z>p.x?p=p.zyx:p,
		p.z=1.5-abs(p.z-1.3+sin(p.z)*.2),
		p.y>p.x?p=p.yxz:p,
		p.x=3.-abs(p.x-5.+sin(p.x*3.)*.2),
		p.y>p.x?p=p.yxz:p,
		p.y=.9-abs(p.y-.4),
		e=12.*clamp(.3/min(dot(p,p),1.),.0,1.)+
		2.*clamp(.1/min(dot(p,p),1.),.0,1.),
		p=e*p-vec3(7,1,1),
		s*=e;
	return length(p)/s;
}

float deGazTemple2(vec3 p){
	for(int j=0;++j<8;)
		p.z-=.3,
		p.xz=abs(p.xz),
		p.xz=(p.z>p.x)?p.zx:p.xz,
		p.xy=(p.y>p.x)?p.yx:p.xy,
		p.z=1.-abs(p.z-1.),
		p=p*3.-vec3(10,4,2);
	return length(p)/6e3-.001;
}

float deGazTemple3( vec3 p ){
	p.z-=2.5;
	float s = 3.;
	float e = 0.;
	for(int j=0;j++<8;)
		s*=e=3.8/clamp(dot(p,p),0.,2.),
		p=abs(p)*e-vec3(1,15,1);
	return length(cross(p,vec3(1,1,-1)*.577))/s;
}

float deGazTemple4( vec3 p ){
	vec3 a=vec3(.5);
	p.z-=55.; p = abs(p);
	float s=2., l=0.;
	for(int j=0;j++<8;)
		p=-sign(p)*(abs(abs(abs(p)-2.)-1.)-1.),
		s*=l=-2.12/max(.2,dot(p,p)),
		p=p*l-.55;
	return dot(p,a)/s;
}

#define fold45(p)(p.y>p.x)?p.yx:p
float deGazTemple5( vec3 p ) {
	float scale = 2.1, off0 = .8, off1 = .3, off2 = .83;
	vec3 off =vec3(2.,.2,.1);
	float s=1.0;
	for(int i = 0;++i<20;) {
		p.xy = abs(p.xy);
		p.xy = fold45(p.xy);
		p.y -= off0;
		p.y = -abs(p.y);
		p.y += off0;
		p.x += off1;
		p.xz = fold45(p.xz);
		p.x -= off2;
		p.xz = fold45(p.xz);
		p.x += off1;
		p -= off;
		p *= scale;
		p += off;
		s *= scale;
	}
	return length(p)/s;
}

float deRingo ( vec3 p ) {
  float e, s, t=0.0; // time adjust term
  vec3 q=p;
  p.z+=7.;
  p=vec3(log(s=length(p)),atan(p.y,p.x),sin(t/4.+p.z/s));
  s=1.;
  for(int j=0;j++<6;)
    s*=e=PI/min(dot(p,p),.8),
    p=abs(p)*e-3.,
    p.y-=round(p.y);
  return e=length(p)/s;
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

float deSquiggles(vec3 p){
	float k = pi*2.;
	vec3 v = vec3(0.,3.,fract(k));
	return (length(cross(cos(p+v),p.zxy))-0.4)*0.2;
}

vec3 pmin ( vec3 a, vec3 b, vec3 k ) {
  vec3 h = clamp( 0.5 + 0.5 * ( b - a ) / k, 0.0, 1.0 );
  return mix( b, a, h ) - k * h * ( 1.0 - h );
}
void sphere_fold ( inout vec3 z, inout float dz ) {
  const float fixed_radius2 = 1.9;
  const float min_radius2 = 0.5;
  float r2 = dot( z, z );
  if ( r2 < min_radius2 ) {
    float temp = ( fixed_radius2 / min_radius2 );
    z *= temp;
    dz *= temp;
  } else if ( r2 < fixed_radius2 ) {
    float temp = ( fixed_radius2 / r2 );
    z *= temp;
    dz *= temp;
  }
}
void box_fold(float k, inout vec3 z, inout float dz) {
  vec3 zz = sign( z ) * pmin( abs( z ), vec3( 1.0 ), vec3( k ) );
  z = zz * 2.0 - z;
}
float sphere ( vec3 p, float t ) {
  return length( p ) - t;
}
float boxf ( vec3 p, vec3 b, float e ) {
  p = abs( p ) - b;
  vec3 q = abs( p + e ) - e;
  return min( min(
    length( max( vec3( p.x, q.y, q.z ), 0.0 ) ) + min( max( p.x, max( q.y, q.z ) ), 0.0 ),
    length( max( vec3( q.x, p.y, q.z ), 0.0 ) ) + min( max( q.x, max( p.y, q.z ) ), 0.0 ) ),
    length( max( vec3( q.x, q.y, p.z ), 0.0 ) ) + min( max( q.x, max( q.y, p.z ) ), 0.0 ) );
}
float deGorp ( vec3 z ) {
  const float scale = -2.8;
  vec3 offset = z;
  float dr = 1.0;
  float fd = 0.0;
  const float k = 0.05;
  for ( int n = 0; n < 5; ++n ) {
    box_fold( k / dr, z, dr );
    sphere_fold( z, dr );
    z = scale * z + offset;
    dr = dr * abs( scale ) + 1.0;
    float r1 = sphere( z, 5.0 );
    float r2 = boxf( z, vec3( 5.0 ), 0.5 );
    float r = n < 4 ? r2 : r1;
    float dd = r / abs( dr );
    if ( n < 3 || dd < fd ) {
      fd = dd;
    }
  }
  return fd;
}


float deAnemone(vec3 p){
	#define V vec2(.7,-.7)
	#define G(p)dot(p,V)
	float i=0.,g=0.,e=1.;
	float t = 0.34; // change to see different behavior
	for(int j=0;j++<8;){
		p=abs(rotate3D(0.34,vec3(1,-3,5))*p*2.)-1.,
		p.xz-=(G(p.xz)-sqrt(G(p.xz)*G(p.xz)+.05))*V;
	}
	return length(p.xz)/3e2;
}

//=============================================================================================================================
#include "oldTestChamber.h.glsl"
//=============================================================================================================================
float de( in vec3 p ) {
	vec3 pOriginal = p;
	float sceneDist = 1000.0f;
	hitColor = vec3( 0.0f );
	hitSurfaceType = NOHIT;
	hitRoughness = 0.0f;

	// form for the following:
	// {
		// const d = blah whatever SDF
		// sceneDist = min( sceneDist, d )
		// if ( sceneDist == d && d < epsilon ) {
			// set material specifics - hitColor, hitSurfaceType, hitRoughness
		// }
	// }

	// pModInterval1( p.x, 1.0f, -5.0f, 5.0f );
	// pModInterval1( p.y, 4.0f, -2.0f, 2.0f );

	// {
	// 	// const float d = deFractal( p );
	// 	const float d = max( deRingo( p ), distance( p, vec3( 0.0f ) ) - marbleRadius - 0.001f );
	// 	sceneDist = min( sceneDist, d );
	// 	if ( sceneDist == d && d < epsilon ) {
	// 		hitSurfaceType = NormalizedRandomFloat() < 0.9f ? DIFFUSE : MIRROR;
	// 		// hitSurfaceType = DIFFUSE;
	// 		// hitColor = vec3( 0.713f, 0.170f, 0.026f ); // carrot
	// 		// hitColor = vec3( 0.831f, 0.397f, 0.038f ); // honey
	// 		// hitColor = vec3( 0.887f, 0.789f, 0.434f ); // bone
	// 		// hitColor = vec3( 0.023f, 0.023f, 0.023f ); // tire
	// 		// hitColor = vec3( 0.670f, 0.764f, 0.855f ); // sapphire blue
	// 		hitColor = hitSurfaceType == DIFFUSE ? vec3( 0.649f, 0.610f, 0.541f ) : vec3( 0.797f, 0.801f, 0.789f ); // nickel + specular coloring
	// 	}
	// }

	// {
	// 	const float d = deGazTemple2( p );
	// 	sceneDist = min( sceneDist, d );
	// 	if ( sceneDist == d && d < epsilon ) {
	// 		hitSurfaceType = NormalizedRandomFloat() < 0.9f ? DIFFUSE : MIRROR;
	// 		hitColor = vec3( 0.887f, 0.789f, 0.434f ); // bone
	// 	}
	// }

	// {
	// 	const float d = deGazTemple1( p );
	// 	sceneDist = min( sceneDist, d );
	// 	if ( sceneDist == d && d < epsilon ) {
	// 		hitSurfaceType = NormalizedRandomFloat() < 0.9f ? DIFFUSE : MIRROR;
	// 		hitColor = vec3( 0.023f, 0.023f, 0.023f ); // tire
	// 	}
	// }

	// {
	// 	const float d = deGazTemple4( p );
	// 	sceneDist = min( sceneDist, d );
	// 	if ( sceneDist == d && d < epsilon ) {
	// 		hitSurfaceType = MIRROR;
	// 		hitColor = vec3( 0.618f );
	// 	}
	// }

	{
		const float d = max( deLeaf( p ), distance( p, vec3( 0.0f ) ) - marbleRadius - 0.4f );
		sceneDist = min( sceneDist, d );
		if ( sceneDist == d && d < epsilon ) {
			// hitSurfaceType = METALLIC;
			// hitRoughness = 0.5f;
			hitSurfaceType = NormalizedRandomFloat() < 0.9f ? DIFFUSE : MIRROR;
			// hitColor = vec3( 0.831f, 0.397f, 0.038f ); // honey
			// hitColor = hitSurfaceType == DIFFUSE ? vec3( 0.670f, 0.764f, 0.855f ) : vec3( 0.797f, 0.201f, 0.1f );
			// hitColor = hitSurfaceType == DIFFUSE ? vec3( 0.023f, 0.023f, 0.023f ) : vec3( 0.831f, 0.397f, 0.038f );
			hitColor = hitSurfaceType == DIFFUSE ? vec3( 0.23f, 0.23f, 0.23f ) : vec3( 0.831f, 0.107f, 0.038f );
		}
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
	intersection.roughness = hitRoughness;
	intersection.normal = SDFNormal( ray.origin + dTotal * ray.direction );
	intersection.frontfaceHit = ( dot( intersection.normal, ray.direction ) <= 0.0f );
	return intersection;
}
//=============================================================================================================================
//	explicit intersection primitives
//		refactor this, or put it in a header, it's a bunch of ugly shit
//=============================================================================================================================
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
uniform vec3 ddaSpheresBoundSize;
uniform int ddaSpheresResolution;
layout( rgba8ui ) readonly uniform uimage3D DDATex;
layout( r32f ) readonly uniform image2D HeightmapTex;
//=============================================================================================================================
vec3 GetPositionForIdx( ivec3 idx ) {
	return ( vec3( idx ) - vec3( ddaSpheresResolution / 2.0f ) ) * 0.009f + vec3( 6.5f );
}

float ddaDiskSDF( vec3 p ) {
	// return deBB( p );
	return deLeaf( p );
	// return max( deFractal2( p ), ( distance( p, vec3( 0.0f ) ) - ( ddaSpheresResolution / 200.0f ) ) );
}

bool CheckValidityOfIdx( ivec3 idx ) {
	// return true;

	// return snoise3D( idx * 0.01f ) < 0.0f;

	// bool mask = deStairs( idx * 0.01f - vec3( 2.9f ) ) < 0.001f;
	// bool mask = deWater( idx * 0.02f - vec3( 3.5f ) ) < 0.01f;
	// bool mask = ddaDiskSDF( GetPositionForIdx( idx ) ) < 0.01f;

	bool mask = ( imageLoad( DDATex, idx ).a != 0 );

	// const float heightSample = imageLoad( HeightmapTex, idx.xy ).r * 512;
	// const float scaledZAxis = float( idx.z );
	// bool mask = heightSample < scaledZAxis;

	// bool blackOrWhite = ( step( -0.0f,
	// 	cos( PI * 0.01f * float( idx.x ) + PI / 2.0f ) *
	// 	cos( PI * 0.01f * float( idx.y ) + PI / 2.0f ) *
	// 	cos( PI * 0.01f * float( idx.z ) + PI / 2.0f ) ) == 0 );

	// return true;
	return mask;
	// return blackOrWhite && mask;
	// return blackOrWhite;
}
//=============================================================================================================================
vec3 GetOffsetForIdx( ivec3 idx ) {
	return vec3( 0.5f );

	// jitter... not liking the look of this, much
	// return vec3( 0.26f ) + 0.45f * ( pcg3d( uvec3( idx ) ) / 4294967296.0f );
	// return vec3( NormalizedRandomFloat(), NormalizedRandomFloat(), NormalizedRandomFloat() );
}
//=============================================================================================================================
float GetRadiusForIdx( ivec3 idx ) {
	return 0.47f;
	// return 0.37f;
	// return 0.4f;
	// return 0.2f;

	// return 0.45f * ( pcg3d( uvec3( idx ) ) / 4294967296.0f ).x;
	// return NormalizedRandomFloat() / 2.0f;

	// return saturate( ( snoise3D( idx * 0.01f ) / 2.0f ) + 1.0f ) / 2.0f;
	// return saturate( snoise3D( idx * 0.04f ) / 4.0f );
}
//=============================================================================================================================
uint GetAlphaValueForIdx( ivec3 idx ) {
	return imageLoad( DDATex, idx ).a;
}
//=============================================================================================================================
vec3 GetColorForIdx( ivec3 idx ) {
	// return vec3( 0.23f, 0.618f, 0.123f ).ggg;
	// return vec3( 0.99f );
	// return viridis( ( pcg3d( uvec3( idx ) ).x / 4294967296.0f ) / 5.0f + 0.6f );
	// return vec3( pcg3d( uvec3( idx ) ) / 4294967296.0f ) + vec3( 0.8f );
	// return mix( vec3( 0.99f ), vec3( 0.99, 0.6f, 0.1f ), pcg3d( uvec3( idx ) ).x / 4294967296.0f );
	// return mix( vec3( 0.618f ), vec3( 0.0, 0.0f, 0.0f ), saturate( pcg3d( uvec3( idx ) ) / 4294967296.0f ) );
	return imageLoad( DDATex, idx ).rgb / 255.0f;
	// return magma( imageLoad( HeightmapTex, idx.xy ).r );
}
//=============================================================================================================================
int GetMaterialIDForIdx( ivec3 idx ) {
	// return DIFFUSE;
	// return MIRROR;
	// return REFRACTIVE;
	// return NormalizedRandomFloat() < 0.9f ? MIRROR : DIFFUSE;
	return int( imageLoad( DDATex, idx ).a );
}
//=============================================================================================================================
intersection_t DDATraversal( in ray_t ray, in float distanceToBounds ) {
	ray_t rayCache = ray;

	// accounting for the bounding box
	if ( distanceToBounds > 0.0f ) {
		ray.origin += ray.direction * distanceToBounds;
	}

	// map the ray into the integer grid space
	vec3 scales = ddaSpheresBoundSize / max( ddaSpheresBoundSize.x, max( ddaSpheresBoundSize.y, ddaSpheresBoundSize.z ) );
	const vec3 res = scales * vec3( ddaSpheresResolution );

	const float epsilon = 0.001f;
	ray.origin = vec3(
		RangeRemapValue( ray.origin.x, -ddaSpheresBoundSize.x / 2.0f, ddaSpheresBoundSize.x / 2.0f, epsilon, res.x - epsilon ),
		RangeRemapValue( ray.origin.y, -ddaSpheresBoundSize.y / 2.0f, ddaSpheresBoundSize.y / 2.0f, epsilon, res.y - epsilon ),
		RangeRemapValue( ray.origin.z, -ddaSpheresBoundSize.z / 2.0f, ddaSpheresBoundSize.z / 2.0f, epsilon, res.z - epsilon )
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
	for ( int i = 0; i < MAX_RAY_STEPS && ( all( greaterThanEqual( mapPos0, ivec3( 0 ) ) ) && all( lessThan( mapPos0, ivec3( res ) ) ) ); i++ ) {
	// for ( int i = 0; i < MAX_RAY_STEPS; i++ ) {

		// Core of https://www.shadertoy.com/view/4dX3zl Branchless Voxel Raycasting
		bvec3 mask1 = lessThanEqual( sideDist0.xyz, min( sideDist0.yzx, sideDist0.zxy ) );
		vec3 sideDist1 = sideDist0 + vec3( mask1 ) * deltaDist;
		ivec3 mapPos1 = mapPos0 + ivec3( vec3( mask1 ) ) * rayStep;

		// checking mapPos0 for hit
		if ( CheckValidityOfIdx( mapPos0 ) ) {

// changing behavior of the traversal - the disks are not super compatible with the sphere/box intersection code
#define SPHEREORBOX
// #define DISKS

			#ifdef SPHEREORBOX
				// see if we found an intersection - ball will almost fill the grid cell
				iqIntersect test = IntersectSphere( ray, vec3( mapPos0 ) + GetOffsetForIdx( mapPos0 ), GetRadiusForIdx( mapPos0 ) );
				// iqIntersect test = IntersectBox( ray, vec3( mapPos0 ) + vec3( 0.5f ), vec3( 0.5f ) );
				const bool behindOrigin = ( test.a.x < 0.0f && test.b.x < 0.0f );

				// update ray, to indicate hit location
				if ( !IsEmpty( test ) && !behindOrigin ) {
					intersection.frontfaceHit = !( test.a.x < 0.0f && test.b.x >= 0.0f );

					// get the location of the intersection
					ray.origin = ray.origin + ray.direction * ( intersection.frontfaceHit ? test.a.x : test.b.x );

					ray.origin = vec3( // map the ray back into the world space
						RangeRemapValue( ray.origin.x, epsilon, res.x - epsilon, -ddaSpheresBoundSize.x / 2.0f, ddaSpheresBoundSize.x / 2.0f ),
						RangeRemapValue( ray.origin.y, epsilon, res.y - epsilon, -ddaSpheresBoundSize.y / 2.0f, ddaSpheresBoundSize.y / 2.0f ),
						RangeRemapValue( ray.origin.z, epsilon, res.z - epsilon, -ddaSpheresBoundSize.z / 2.0f, ddaSpheresBoundSize.z / 2.0f )
					);

					intersection.dTravel = distance( ray.origin, rayCache.origin );
					intersection.normal = intersection.frontfaceHit ? test.a.yzw : test.b.yzw;
					// intersection.materialID = GetMaterialIDForIdx( mapPos0 );

					vec3 testVal = ( pcg3d( uvec3( mapPos0 ) ) / 4294967296.0f );
					// vec3 testVal = vec3( 1.0f );
					// if ( testVal.x < 0.0004f ) {
					// 	intersection.materialID = EMISSIVE;
					// 	intersection.albedo = 3.0f * ( vec3( 0.618f ) + inferno( testVal.g ) );
					// } else if ( testVal.x < 0.1f ) {
					// 	intersection.materialID = DIFFUSE;
					// 	intersection.albedo = vec3( 0.618f );
					// } else {
						// intersection.materialID = MIRROR;
						intersection.materialID = intersection.frontfaceHit ? REFRACTIVE : REFRACTIVE_BACKFACE;
						intersection.albedo = GetColorForIdx( mapPos0 );
						// intersection.albedo = vec3( 0.99f );
						// intersection.roughness = 0.5f;
						// intersection.roughness = 0.0f;
						intersection.roughness = 0.3f;
						// intersection.materialID = DIFFUSE;
					// }

					// intersection.materialID = NormalizedRandomFloat() < 0.9f ? MIRROR : DIFFUSE;
					// if ( GetAlphaValueForIdx( mapPos0 ) < 35 ) {
					// 	// intersection.albedo *= 3.0f;
					// 	// intersection.materialID = EMISSIVE;
					// 	// intersection.materialID = DIFFUSE;
					// 	intersection.materialID = intersection.frontfaceHit ? REFRACTIVE : REFRACTIVE_BACKFACE;
					// } else if ( GetAlphaValueForIdx( mapPos0 ) < 130 ) {
					// 	intersection.materialID = MIRROR;
					// 	// intersection.materialID = NormalizedRandomFloat() > 0.9f ? MIRROR : DIFFUSE;
					// 	// intersection.materialID = intersection.frontfaceHit ? REFRACTIVE : REFRACTIVE_BACKFACE;
					// } else {
					// 	intersection.materialID = DIFFUSE;
					// }
					break;
				}
			#endif

			#ifdef DISKS
				// disk test
				const vec3 planePt = vec3( mapPos0 ) + vec3( 0.5f );
				vec2 e = vec2( epsilon, 0.0f );
				vec3 p = GetPositionForIdx( mapPos0 );
				const vec3 normal = normalize(
					vec3( ddaDiskSDF( p ) ) - vec3(
						vec3( ddaDiskSDF( p - e.xyy ), ddaDiskSDF( p - e.yxy ), ddaDiskSDF( p - e.yyx ) )
					)
				);

				const float planeDistance = -( dot( ray.origin - planePt, normal ) ) / dot( ray.direction, normal );
				const vec3 hitPointInPlane = ray.origin + ray.direction * planeDistance;
				const float radius = 0.5f;

				if ( distance( planePt, hitPointInPlane ) < radius ) {

					// get the location of the intersection
					ray.origin = hitPointInPlane;

					ray.origin = vec3( // map the ray back into the world space
						RangeRemapValue( ray.origin.x, epsilon, res.x - epsilon, -ddaSpheresBoundSize.x / 2.0f, ddaSpheresBoundSize.x / 2.0f ),
						RangeRemapValue( ray.origin.y, epsilon, res.y - epsilon, -ddaSpheresBoundSize.y / 2.0f, ddaSpheresBoundSize.y / 2.0f ),
						RangeRemapValue( ray.origin.z, epsilon, res.z - epsilon, -ddaSpheresBoundSize.z / 2.0f, ddaSpheresBoundSize.z / 2.0f )
					);

					intersection.dTravel = distance( ray.origin, rayCache.origin );
					intersection.frontfaceHit = dot( normal, ray.direction ) < 0.0f;
					// intersection.normal = intersection.frontfaceHit ? normal : -normal;
					intersection.normal = intersection.frontfaceHit ? -normal : normal;
					intersection.roughness = 0.01f;
					intersection.IoR = 1.0f / 1.5f;
					// intersection.materialID = GetMaterialIDForIdx( mapPos0 );
					// intersection.materialID = intersection.frontfaceHit ? REFRACTIVE : REFRACTIVE_BACKFACE;
					// intersection.materialID = intersection.frontfaceHit ? REFRACTIVE : MIRROR;
					intersection.materialID = DIFFUSE;
					// intersection.materialID = NormalizedRandomFloat() > 0.9f ? MIRROR : DIFFUSE;
					intersection.albedo = GetColorForIdx( mapPos0 );
					break;
				}
			#endif
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
uniform bool maskedPlaneEnable;
layout( rgba8ui ) readonly uniform uimage3D textBuffer;
//=============================================================================================================================
bool maskedPlaneMaskEval( in vec3 location, out vec3 color ) {
	// bool mask = ( step( -0.0f, // placeholder, glyph mapping + texture stuff next
	// 	cos( PI * float( location.x ) + PI / 2.0f ) *
	// 	cos( PI * float( location.y ) + PI / 2.0f ) ) == 0 );

	ivec2 pixelIndex = ivec2( floor( location.xy ) );
	ivec2 bin = ivec2( floor( pixelIndex.xy / vec2( 8.0f, 16.0f ) ) );
	ivec2 offset = ivec2( pixelIndex.xy ) % ivec2( 8, 16 );

	// return ( step( -0.0f, // placeholder, glyph mapping + texture stuff next
	// 	cos( PI * float( pixelIndex.x ) + PI / 2.0f ) *
	// 	cos( PI * float( pixelIndex.y ) + PI / 2.0f ) ) == 0 );

	const uvec3 texDims = uvec3( imageSize( textBuffer ) );
	// uvec4 sampleValue = imageLoad( textBuffer, ivec3( bin.x % texDims.x, bin.y % texDims.y, location.z ) ); // repeated
	uvec4 sampleValue = imageLoad( textBuffer, ivec3( bin.x, bin.y % texDims.y, location.z ) );

	color = vec3( sampleValue.xyz ) / 255.0f;

	// int onGlyph = fontRef( int( abs( snoise3D( vec3( bin.xy, location.z ) ) ) * 255 ), offset );
	int onGlyph = fontRef( int( sampleValue.a ), offset );
	return onGlyph > 0;
}
//=============================================================================================================================
intersection_t MaskedPlaneIntersect( in ray_t ray, in mat3 transform, in vec3 basePoint, in ivec3 dims, in vec3 scale ) {
	intersection_t intersection = DefaultIntersection();

	// determine the direction of travel, so that we know what order to step through the planes in
	vec3 normal = transform * vec3( 0.0f, 0.0f, 1.0f );
	const float rayDot = dot( ray.direction, normal );

	// if the ray is parallel, we can early out with a nohit default intersection
	if ( rayDot == 0.0f ) {
		return intersection;
	}

	// for x,y mapping on the plane
	const vec3 xVec = transform * vec3( 1.0f, 0.0f, 0.0f );
	const vec3 yVec = transform * vec3( 0.0f, 1.0f, 0.0f );

	// is this ray going in the same direction as the normal vector
	const bool nAlign = ( rayDot < 0.0f );
	const int start  = nAlign ? dims.z - 1 : 0;
	const int thresh = nAlign ? 0 : dims.z - 1;
	// iterating through the planes - top to bottom, or bottom to top
	for ( int i = start; nAlign ? ( i >= thresh ) : ( i <= thresh ); nAlign ? ( i-- ) : ( i++ ) ) {
		// do the plane test
		const vec3 planePt = i * scale.z * normal + basePoint;
		const float planeDistance = -( dot( ray.origin - planePt, normal ) ) / dot( ray.direction, normal );
		if ( planeDistance > 0.0f ) {

			// get the 2d location on the plane
			const vec3 planeIntersection = ray.origin + ray.direction * planeDistance;
			const vec3 intersectionPointInPlane = planeIntersection - planePt;
			const vec2 projectedCoords = vec2( // vector projected onto vector ( +x,y scale factors )
				scale.x * dot( intersectionPointInPlane, xVec ) + imageSize( textBuffer ).x / 2,
				scale.y * dot( intersectionPointInPlane, yVec ) + imageSize( textBuffer ).y / 2
			);

			// check the projected coordinates against the input boundary size
			if ( abs( projectedCoords.x ) <= float( dims.x * scale.x ) / 2.0f && abs( projectedCoords.y ) <= float( dims.y * scale.y ) / 2.0f ) {
				// now we have a 2d point to work with
				vec3 color;
				if ( maskedPlaneMaskEval( vec3( projectedCoords, i ), color ) ) {
					intersection.dTravel = planeDistance;
					intersection.albedo = color;
					intersection.frontfaceHit = nAlign;
					intersection.normal = nAlign ? normal : -normal;
					intersection.frontfaceHit = true;
					intersection.materialID = DIFFUSE;
					break; // we can actually early out the first time we get a good hit, passing the mask test, since we are going in order
				}
			}
		}
	}
	// fall out of the loop and return
	return intersection;
}
//=============================================================================================================================
intersection_t MaskedPlaneIntersect( in ray_t ray ) {
	mat3 basis = mat3( 1.0f ); // identity matrix - should be able to support rotation
	const vec3 point = vec3( 0.0f ); // base point for the plane
	const ivec3 dims = ivec3( imageSize( textBuffer ) ); // "voxel" dimensions - x,y glyph res + z number of planes
	const vec3 scale = vec3( 1000.0f, 1000.0f, 0.003f ); // scaling x,y, and z is spacing between the planes
	// return MaskedPlaneIntersect( ray, basis, point, dims, scale );

	intersection_t intersect1 = MaskedPlaneIntersect( ray, basis, point, dims, scale );

	basis = Rotate3D( 1.5f, vec3( 0.0f, 1.0f, 0.0f ) ) * Rotate3D( 0.5f, vec3( 1.0f, 1.0f, 0.0f ) ) * basis;
	intersection_t intersect2 = MaskedPlaneIntersect( ray, basis, point, dims, scale );

	basis = Rotate3D( 1.5f, vec3( 0.0f, 1.0f, 0.0f ) ) * Rotate3D( 2.5f, vec3( 1.0f, 1.0f, 0.0f ) ) * basis;
	intersection_t intersect3 = MaskedPlaneIntersect( ray, basis, point, dims, scale );

	intersection_t result = DefaultIntersection();
	float minDistance = vmin( vec3( intersect1.dTravel, intersect2.dTravel, intersect3.dTravel ) );
	if ( minDistance == intersect1.dTravel ) {
		result = intersect1;
	} else if ( minDistance == intersect2.dTravel ) {
		result = intersect2;
	} else {
		result = intersect3;
	}
	return result;
}
//=============================================================================================================================
// 
//=============================================================================================================================
// CPU-generated sphere array
	// I want to generalize this:
		// primitive type
		// primitive parameters
		// material details
uniform int numExplicitPrimitives;
uniform bool explicitListEnable;
struct sphere_t {
	vec4 positionRadius;
	vec4 colorMaterial;
	vec4 materialProps;
};
layout( binding = 0, std430 ) readonly buffer sphereData {
	sphere_t spheres[];
};
//=============================================================================================================================
intersection_t ExplicitListIntersect( in ray_t ray ) {
	iqIntersect iResult = kEmpty;
	int indexOfHit = -1;
	float nearestOverallHit = 1e20f;
	for ( int i = 0; i < numExplicitPrimitives; i++ ) {
		iqIntersect current = IntersectSphere( ray, spheres[ i ].positionRadius.xyz, spheres[ i ].positionRadius.w );
		// iqIntersect current = ( i % 2 == 0 ) ? IntersectBox( origin, direction, spheres[ i ].positionRadius.xyz, vec3( spheres[ i ].positionRadius.w ) )
			// : IntersectSphere( origin, direction, spheres[ i ].positionRadius.xyz, spheres[ i ].positionRadius.w );
		const float currentNearestPositive = ( current.a.x < 0.0f ) ? ( current.b.x < 0.0f ) ? 1e20f : current.b.x : current.a.x;
		nearestOverallHit = min( currentNearestPositive, nearestOverallHit );
		if ( currentNearestPositive == nearestOverallHit ) {
			iResult = current;
			indexOfHit = i;
		}
	}
	intersection_t result = DefaultIntersection();
	if ( indexOfHit != -1 ) { // at least one primitive got a valid hit
		result.dTravel = nearestOverallHit;
		result.frontfaceHit = ( iResult.a.x == nearestOverallHit );
		result.normal = result.frontfaceHit ? iResult.a.yzw : iResult.b.yzw;
		result.materialID = int( spheres[ indexOfHit ].colorMaterial.w );
		if ( result.materialID == REFRACTIVE && result.frontfaceHit == false )
			result.materialID = REFRACTIVE_BACKFACE; // this needs to be more generalized
		result.albedo = spheres[ indexOfHit ].colorMaterial.xyz;
		result.IoR = spheres[ indexOfHit ].materialProps.r;
		result.roughness = spheres[ indexOfHit ].materialProps.g;
	}
	return result;
}
//=============================================================================================================================
intersection_t GetNearestSceneIntersection( in ray_t ray ) {
	// return a single intersection_t representing the closest ray intersection
	intersection_t SDFResult		= raymarchEnable		? raymarch( ray )				: DefaultIntersection();
	intersection_t VoxelResult		= ddaSpheresEnable		? VoxelIntersection( ray )		: DefaultIntersection();
	intersection_t MaskedPlane		= maskedPlaneEnable		? MaskedPlaneIntersect( ray )	: DefaultIntersection();
	intersection_t ExplicitList		= explicitListEnable	? ExplicitListIntersect( ray )	: DefaultIntersection();

	intersection_t result = DefaultIntersection();
	float minDistance = vmin( vec4( SDFResult.dTravel, VoxelResult.dTravel, MaskedPlane.dTravel, ExplicitList.dTravel ) );
	if ( minDistance == SDFResult.dTravel ) {
		result = SDFResult;
	} else if ( minDistance == VoxelResult.dTravel ) {
		result = VoxelResult;
	} else if ( minDistance == MaskedPlane.dTravel ) {
		result = MaskedPlane;
	} else if ( minDistance == ExplicitList.dTravel ) {
		result = ExplicitList;
		// result.IoR = 1.0f / 1.7f; // hack to make marble IoR constant
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

		// attenuate by some amount times the distance traveled... tbd.. this kind of sucks
		// throughput = throughput * exp( -0.03f * intersection.dTravel );

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
	imageStore( accumulatorColor, pixel, clamp( mixedColor, 0.0f, 10000.0f ) );
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
#version 430 core
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( rgba32f ) uniform image2D accumulatorColor;
layout( rgba32f ) uniform image2D accumulatorNormalsAndDepth;
layout( rgba8ui ) uniform uimage2D blueNoise;

#include "hg_sdf.glsl" // SDF modeling functions

// offsets
uniform ivec2 tileOffset;
uniform ivec2 noiseOffset;

// more general parameters
uniform int wangSeed;
uniform float exposure;
uniform float FoV;
uniform vec3 viewerPosition;
uniform vec3 basisX;
uniform vec3 basisY;
uniform vec3 basisZ;

// raymarch parameters
uniform int raymarchMaxSteps;
uniform int raymarchMaxBounces;
uniform float raymarchMaxDistance;
uniform float raymarchEpsilon;
uniform float raymarchUnderstep;

// scene parameters
uniform vec3 skylightColor;

mat3 Rotate3D ( float angle, vec3 axis ) {
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

// random utilites
uint seed = 0;
uint WangHash () {
	seed = uint( seed ^ uint( 61 ) ) ^ uint( seed >> uint( 16 ) );
	seed *= uint( 9 );
	seed = seed ^ ( seed >> 4 );
	seed *= uint( 0x27d4eb2d );
	seed = seed ^ ( seed >> 15 );
	return seed;
}

float NormalizedRandomFloat () {
	return float( WangHash() ) / 4294967296.0f;
}

vec3 RandomUnitVector () {
	const float z = NormalizedRandomFloat() * 2.0f - 1.0f;
	const float a = NormalizedRandomFloat() * 2.0f * PI;
	const float r = sqrt( 1.0f - z * z );
	const float x = r * cos( a );
	const float y = r * sin( a );
	return vec3( x, y, z );
}

vec2 RandomInUnitDisk () {
	return RandomUnitVector().xy;
}

vec3 GetColorForTemperature ( float temperature ) {
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

float Reflectance ( float cosTheta, float IoR ) {
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

bool BoundsCheck ( ivec2 loc ) {
	// used to abort off-image samples
	const ivec2 bounds = ivec2( imageSize( accumulatorColor ) ).xy;
	return ( loc.x < bounds.x && loc.y < bounds.y && loc.x >= 0 && loc.y >= 0 );
}

// vec3 getCameraRayForUV ( vec2 uv ) {
// 	// placeholder for switchable cameras ( fisheye, etc )
// 	return vec3( uv, 1.0f );
// }

vec2 SubpixelOffset () {
	// tbd, probably blue noise
	return vec2( NormalizedRandomFloat(), NormalizedRandomFloat() );
}

// ==============================================================================================

// organic shape
float deOrganic ( vec3 p ) {
	float S = 1.0f;
	float R, e;
	float time = 0.0f;
	p.y += p.z;
	p = vec3( log( R = length( p ) ) - time, asin( -p.z / R ), atan( p.x, p.y ) + time );
	for ( e = p.y - 1.5f; S < 6e2; S += S ) {
		e += sqrt( abs( dot( sin( p.zxy * S ), cos( p * S ) ) ) ) / S;
	}
	return e * R * 0.1f;
}

float deC ( vec3 p ) {
	const vec3 va = vec3(  0.0f,  0.57735f,  0.0f );
	const vec3 vb = vec3(  0.0f, -1.0f,  1.15470f );
	const vec3 vc = vec3(  1.0f, -1.0f, -0.57735f );
	const vec3 vd = vec3( -1.0f, -1.0f, -0.57735f );
	float a = 0.0f;
	float s = 1.0f;
	float r = 1.0f;
	float dm;
	vec3 v;
	for ( int i = 0; i < 16; i++ ) {
		float d, t;
		d = dot( p - va, p - va );                v = va; dm = d; t = 0.0f;
		d = dot( p - vb, p - vb ); if( d < dm ) { v = vb; dm = d; t = 1.0f; }
		d = dot( p - vc, p - vc ); if( d < dm ) { v = vc; dm = d; t = 2.0f; }
		d = dot( p - vd, p - vd ); if( d < dm ) { v = vd; dm = d; t = 3.0f; }
		p = v + 2.0f * ( p - v ); r *= 2.0f;
		a = t + 4.0f * a;
		s*= 4.0f;
	}
	return ( sqrt( dm ) - 1.0f ) / r;
}


float deG ( vec3 p0 ) {
	vec4 p = vec4( p0, 3.0f );
	// escape = 0.0f;
	p*= 2.0f / min( dot( p.xyz, p.xyz ), 30.0f );
	for ( int i = 0; i < 14; i++ ) {
		p.xyz = vec3( 2.0f, 4.0f, 2.0f ) - ( abs( p.xyz ) - vec3( 2.0f, 4.0f, 2.0f ) );
		p.xyz = mod( p.xyz - 4.0f, 8.0f ) - 4.0f;
		p *= 9.0f / min( dot( p.xyz, p.xyz ), 12.0f );
		// escape += exp( -0.2f * dot( p.xyz, p.xyz ) );
	}
	p.xyz -= clamp( p.xyz, -1.2f, 1.2f );
	return length( p.xyz ) / p.w;
}

// ==============================================================================================

#define NOHIT		0
#define EMISSIVE	1
#define DIFFUSE		2
#define SPECULAR	3

int hitPointSurfaceType = NOHIT;
vec3 hitPointColor = vec3( 0.0f );

// ==============================================================================================

// overal distance estimate function - the "scene"
	// hitPointSurfaceType gives the type of material
	// hitPointColor gives the albedo of the material

float de ( vec3 p ) {

	hitPointSurfaceType = NOHIT;
	hitPointColor = vec3( 0.0f );
	float sceneDist = 1000.0f;

	const float dOrganic = deOrganic( p );
	const float deC = deC( p );
	const float deG = deG( p );

	sceneDist = min( dOrganic, sceneDist );
	sceneDist = min( deC, sceneDist );
	sceneDist = min( deG, sceneDist );

	if ( sceneDist == dOrganic ) {
		hitPointSurfaceType = DIFFUSE;
		hitPointColor = vec3( 1.0f, 0.5f, 0.2f );
	}

	if ( sceneDist == deC ) {
		hitPointSurfaceType = EMISSIVE;
		hitPointColor = vec3( 0.2f, 0.3f, 0.7f );
	}

	if ( sceneDist == deG ) {
		hitPointSurfaceType = SPECULAR;
		hitPointColor = vec3( 0.24f, 0.09f, 0.3f );
	}

	return sceneDist;
}

// ==============================================================================================

// raymarches to the next hit
float Raymarch ( vec3 origin, vec3 direction ) {
	float dQuery = 0.0f;
	float dTotal = 0.0f;
	for ( int steps = 0; steps < raymarchMaxSteps; steps++ ) {
		vec3 pQuery = origin + dTotal * direction;
		dQuery = de( pQuery );
		dTotal += dQuery * raymarchUnderstep;
		if ( dTotal > raymarchMaxDistance || abs( dQuery ) < raymarchEpsilon ) {
			break;
		}
		// // certain chance to scatter in a random direction, per step - one of Nameless' methods for fog
		// if ( NormalizedRandomFloat() < 0.005f ) { // massive slowdown doing this
		// 	direction = normalize( direction + 0.4f * RandomUnitVector() );
		// }
	}
	return dTotal;
}

vec3 Normal ( in vec3 position ) { // three methods - first one seems most practical
	vec2 e = vec2( raymarchEpsilon, 0.0f );
	return normalize( vec3( de( position ) ) - vec3( de( position - e.xyy ), de( position - e.yxy ), de( position - e.yyx ) ) );

	// vec2 e = vec2( 1.0f, -1.0f ) * raymarchEpsilon;
	// return normalize( e.xyy * de( position + e.xyy ) + e.yyx * de( position + e.yyx ) + e.yxy * de( position + e.yxy ) + e.xxx * de( position + e.xxx ) );

	// vec2 e = vec2( raymarchEpsilon, 0.0f );
	// return normalize( vec3( de( position + e.xyy ) - de( position - e.xyy ), de( position + e.yxy ) - de( position - e.yxy ), de( position + e.yyx ) - de( position - e.yyx ) ) );
}

// fake AO, computed from SDF
float CalcAO ( in vec3 position, in vec3 normal ) {
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

vec3 ColorSample ( const vec2 uvIn ) {
	const float aspectRatio = float( imageSize( accumulatorColor ).x ) / float( imageSize( accumulatorColor ).y );

	// compute initial ray origin, direction
	const vec3 rayOrigin_initial = viewerPosition;
	const vec3 rayDirection_initial = normalize( aspectRatio * uvIn.x * basisX + uvIn.y * basisY + ( 1.0f / FoV ) * basisZ );

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
		// get the hit point
		float dHit = Raymarch( rayOrigin, rayDirection );

		// cache surface type, color so it's not overwritten by normal calcs
		int hitPointSurfaceType_cache = hitPointSurfaceType;
		vec3 hitPointColor_cache = hitPointColor;

		// get previous direction, origin
		rayOriginPrevious = rayOrigin;
		rayDirectionPrevious = rayDirection;

		// update new ray origin ( at hit point )
		rayOrigin = rayOriginPrevious + dHit * rayDirectionPrevious;

		// get the normal vector
		const vec3 hitNormal = Normal( rayOrigin );

		// epsilon bump, along the normal vector
		rayOrigin += 2.0f * raymarchEpsilon * hitNormal;

		// precalculate reflected vector, random diffuse vector, random specular vector
		vec3 reflectedVector = reflect( rayDirectionPrevious, hitNormal );
		vec3 randomVectorDiffuse = normalize( ( 1.0f + raymarchEpsilon ) * hitNormal + RandomUnitVector() );
		vec3 randomVectorSpecular = normalize( ( 1.0f + raymarchEpsilon ) * hitNormal + mix( reflectedVector, RandomUnitVector(), 0.1f ) );

		// add a check for points that are not within epsilon? just ran out the steps?

		if ( dHit >= raymarchMaxDistance ) {
			// ray escaped
			finalColor += throughput * skylightColor;
			break;
		} else {
			// material specific behavior
			switch ( hitPointSurfaceType_cache ) {
			case NOHIT:
				break;

			case EMISSIVE:
				// light emitting material
				finalColor += throughput * hitPointColor_cache;
				break;

			case DIFFUSE:
				// diffuse material
				throughput *= hitPointColor_cache;
				rayDirection = randomVectorDiffuse;
				break;

			case SPECULAR:
				// specular material
				throughput *= hitPointColor_cache;
				rayDirection = randomVectorSpecular;
				break;
			}
		}

		// russian roulette termination
			// also not super critical right now

	}

	return finalColor * exposure;
}

void main () {
	// tiled offset
	uvec2 location = gl_GlobalInvocationID.xy + tileOffset.xy;

	if ( BoundsCheck( ivec2( location ) ) ) {
		// wang hash seeded uniquely for every pixel
		seed = wangSeed + 42069 * location.x + 451 * location.y;

		// subpixel offset, remap uv, etc
		const vec2 uv = ( vec2( location ) + SubpixelOffset() ) / vec2( imageSize( accumulatorColor ).xy );
		const vec2 uvRemapped = 2.0f * ( uv - vec2( 0.5f ) );

			// this is redundant, need to revisit at some point
			const float aspectRatio = float( imageSize( accumulatorColor ).x ) / float( imageSize( accumulatorColor ).y );
			const vec3 rayDirection = normalize( aspectRatio * uvRemapped.x * basisX + uvRemapped.y * basisY + ( 1.0f / FoV ) * basisZ );
			const vec3 rayOrigin = viewerPosition; // potentially expand to enable orthographic, etc
			const float hitDistance = Raymarch( rayOrigin, rayDirection );
			const vec3 hitPoint = rayOrigin + hitDistance * rayDirection;

		// existing values from the buffers
		const vec4 oldColor = imageLoad( accumulatorColor, ivec2( location ) );
		const vec4 oldNormalD = imageLoad( accumulatorNormalsAndDepth, ivec2( location ) );

		// increment the sample count
		const float sampleCount = oldColor.a + 1;

		// new values - color is currently placeholder
		// const vec4 newColor = vec4( vec3( pow( CalcAO( hitPoint, Normal( hitPoint ) ), 5.0f ) ), 1.0f );
		const vec4 newColor = vec4( ColorSample( uvRemapped ), 1.0f );
		const vec4 newNormalD = vec4( Normal( hitPoint ), hitDistance );

		// blended with history based on sampleCount
		const vec4 mixedColor = vec4( mix( oldColor.rgb, newColor.rgb, 1.0f / sampleCount ), sampleCount );
		const vec4 mixedNormalD = vec4( mix( oldNormalD, newNormalD, 1.0f / sampleCount ) );

		// store the values back
		imageStore( accumulatorColor, ivec2( location ), mixedColor );
		imageStore( accumulatorNormalsAndDepth, ivec2( location ), mixedNormalD );
	}
}
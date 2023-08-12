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

// organic shape
float de ( vec3 p ) {
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
		// certain chance to scatter in a random direction, per step - one of Nameless' methods for fog
		if ( NormalizedRandomFloat() < 0.005f ) { // massive slowdown doing this
			direction = normalize( direction + 0.4f * RandomUnitVector() );
		}
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

		// cache surface type, hitpoint color so it's not overwritten by normal calcs
			// not critical right now

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

		// material specific behavior
			// what if, just to simplify the initial setup, just consider distance==max as emissive
		if ( dHit >= raymarchMaxDistance ) {

			// constant white emissive
			finalColor += throughput * vec3( 1.0f );

			// also, not bouncing here, so just break
			break;

		} else {

			// darker grey diffuse material
			throughput *= vec3( 0.18f );
			rayDirection = randomVectorDiffuse;

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
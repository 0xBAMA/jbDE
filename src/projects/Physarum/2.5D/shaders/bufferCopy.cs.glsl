#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform usampler2D continuum;
uniform sampler3D shadedVolume;
uniform vec2 resolution;
uniform vec3 color;
uniform vec3 blockSize;
uniform float brightness;
uniform float scale;
uniform float IoR;
uniform mat3 invBasis;

#include "intersect.h"

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

float RemapRange ( const float value, const float iMin, const float iMax, const float oMin, const float oMax ) {
	return ( oMin + ( ( oMax - oMin ) / ( iMax - iMin ) ) * ( value - iMin ) );
}

float blueNoiseRead ( ivec2 loc, int idx ) {
	ivec2 wrappedLoc = loc % imageSize( blueNoiseTexture );
	uvec4 sampleValue = imageLoad( blueNoiseTexture, wrappedLoc );
	switch ( idx ) {
	case 0:
		return sampleValue.r / 255.0f;
		break;
	case 1:
		return sampleValue.g / 255.0f;
		break;
	case 2:
		return sampleValue.b / 255.0f;
		break;
	case 3:
		return sampleValue.a / 255.0f;
		break;
	}
}

void main () {
	// remapped uv
	vec2 uv = ( vec2( gl_GlobalInvocationID.xy ) + vec2( 0.5f ) ) / resolution.xy;
	uv = ( uv - 0.5f ) * 2.0f;

	// aspect ratio correction
	uv.x *= ( resolution.x / resolution.y );

	// box intersection
	float tMin, tMax;
	vec3 Origin = invBasis * vec3( scale * uv, -2.0f );
	vec3 Direction = invBasis * normalize( vec3( uv * 0.1f, 2.0f ) );

	// first, intersecting with a sphere, to refract
	// const bool sHit = IntersectSphere( Origin, Direction, );
	Intersection sHit = IntersectSphere( Origin, Direction, vec3( 0.0f ), 2.3f );
	if ( !IsEmpty( sHit ) ) {
		// update ray position to be at the sphere's surface
		Origin = Origin + sHit.a.r * Direction;
		// update ray direction to the refracted ray
		Direction = refract( Direction, sHit.a.gba, 1.8f );
	}

	// then intersect with the AABB
	const bool hit = Intersect( Origin, Direction, -blockSize / 2.0f, blockSize / 2.0f, tMin, tMax );

	if ( hit ) { // texture sample
		// for trimming edges
		const float epsilon = -0.003f; 
		const vec3 hitpointMin = Origin + tMin * Direction;
		const vec3 hitpointMax = Origin + tMax * Direction;
		const vec3 blockUVMin = vec3(
			RemapRange( hitpointMin.x, -blockSize.x / 2.0f, blockSize.x / 2.0f, 0.0f - epsilon, 1.0f + epsilon ),
			RemapRange( hitpointMin.y, -blockSize.y / 2.0f, blockSize.y / 2.0f, 0.0f - epsilon, 1.0f + epsilon ),
			RemapRange( hitpointMin.z, -blockSize.z / 2.0f, blockSize.z / 2.0f, 0.0f - epsilon, 1.0f + epsilon )
		);

		const vec3 blockUVMax = vec3(
			RemapRange( hitpointMax.x, -blockSize.x / 2.0f, blockSize.x / 2.0f, 0.0f - epsilon, 1.0f + epsilon ),
			RemapRange( hitpointMax.y, -blockSize.y / 2.0f, blockSize.y / 2.0f, 0.0f - epsilon, 1.0f + epsilon ),
			RemapRange( hitpointMax.z, -blockSize.z / 2.0f, blockSize.z / 2.0f, 0.0f - epsilon, 1.0f + epsilon )
		);

		const float thickness = abs( tMin - tMax );
		const float stepSize = max( thickness / 10.0f, 0.001f );
		const vec3 displacement = normalize( blockUVMax - blockUVMin );

		const int numSteps = 700;
		int iterationsToHit = 0;
		vec3 samplePoint = blockUVMin + blueNoiseRead( ivec2( gl_GlobalInvocationID.xy ), 0 ) * 0.001f;
		vec3 final = vec3( 0.0f );
		for ( ; iterationsToHit < numSteps; iterationsToHit++ ) {
			samplePoint += stepSize * displacement;
			float result = brightness * ( texture( continuum, samplePoint.xy ).r / 1000000.0f );
			if ( result > ( ( samplePoint.z - 0.5f ) * 2.0f ) ) {
				break;
			}
			// vec3 iterationColor = brightness * result * color + vec3( 0.003f );
			vec3 iterationColor = color;
			final = mix( final, iterationColor, 0.01f );
		}
		// final = mix( final, brightness * ( texture( continuum, samplePoint.xy ).r / 1000000.0f ) * color + vec3( 0.003f ), 0.2f );
		// vec3 final = color * exp( -distance( samplePoint, blockUVMax ) );
		imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( final, 1.0f ) );
	} else {
		imageStore( accumulatorTexture, ivec2( gl_GlobalInvocationID.xy ), vec4( 0.0, 0.0f, 0.0f, 1.0f ) );
	}
}
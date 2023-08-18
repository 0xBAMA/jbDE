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

// thin lens parameters
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

vec3 getCameraRayForUV ( const vec2 uv ) {
	// placeholder for switchable cameras ( fisheye, etc )
	return vec3( uv, 1.0f );
}

vec2 SubpixelOffset () {
	// return vec2( NormalizedRandomFloat(), NormalizedRandomFloat() );
	return BlueNoiseReference( ivec2( gl_GlobalInvocationID.xy + tileOffset ) ).xy / 255.0f;
}

// ==============================================================================================

// // organic shape
// float deOrganic ( vec3 p ) {
// 	float S = 1.0f;
// 	float R, e;
// 	float time = 0.0f;
// 	p.y += p.z;
// 	p = vec3( log( R = length( p ) ) - time, asin( -p.z / R ), atan( p.x, p.y ) + time );
// 	for ( e = p.y - 1.5f; S < 6e2f; S += S ) {
// 		e += sqrt( abs( dot( sin( p.zxy * S ), cos( p * S ) ) ) ) / S;
// 	}
// 	return e * R * 0.1f;
// }

// trellis type structure
float deFractal ( vec3 p ) {
	vec3 k = vec3( 5.0f, 2.0f, 1.0f );
	p.y += 5.5f;
	for ( int j = 0; ++j < 8; ) {
		p.xz = abs( p.xz );
		p.xz = p.z > p.x ? p.zx : p.xz;
		p.z = 0.9f - abs( p.z - 0.9f );
		p.xy = p.y > p.x ? p.yx : p.xy;
		p.x -= 2.3f;
		p.xy = p.y > p.x ? p.yx : p.xy;
		p.y += 0.1f;
		p = k + ( p - k ) * 3.2f;
	}
	return length( p ) / 6e3f - 0.001f;
}

// ==============================================================================================
// ====== Old Test Chamber ======================================================================

// float de ( vec3 p ) {
// 	// init nohit, far from surface, no diffuse color
// 	hitPointSurfaceType = NOHIT;
// 	float sceneDist = 1000.0f;
// 	hitPointColor = vec3( 0.0f );

// 	const vec3 whiteWallColor = vec3( 0.618f );
// 	const vec3 floorCielingColor = vec3( 0.9f );

// 	// North, South, East, West walls
// 	const float dNorthWall = fPlane( p, vec3( 0.0f, 0.0f, -1.0f ), 48.0f );
// 	const float dSouthWall = fPlane( p, vec3( 0.0f, 0.0f, 1.0f ), 48.0f );
// 	const float dEastWall = fPlane( p, vec3( -1.0f, 0.0f, 0.0f ), 10.0f );
// 	const float dWestWall = fPlane( p, vec3( 1.0f, 0.0f, 0.0f ), 10.0f );
// 	const float dWalls = fOpUnionRound( fOpUnionRound( fOpUnionRound( dNorthWall, dSouthWall, 0.5f ), dEastWall, 0.5f ), dWestWall, 0.5f );
// 	sceneDist = min( dWalls, sceneDist );
// 	if ( sceneDist == dWalls && dWalls < raymarchEpsilon ) {
// 		hitPointColor = whiteWallColor;
// 		hitPointSurfaceType = DIFFUSE;
// 	}

// 	const float dFloor = fPlane( p, vec3( 0.0f, 1.0f, 0.0f ), 4.0f );
// 	sceneDist = min( dFloor, sceneDist );
// 	if ( sceneDist == dFloor && dFloor < raymarchEpsilon ) {
// 		hitPointColor = floorCielingColor;
// 		hitPointSurfaceType = DIFFUSE;
// 	}

// 	// balcony floor
// 	const float dEastBalcony = fBox( p - vec3( 10.0f, 0.0f, 0.0f ), vec3( 4.0f, 0.1f, 48.0f ) );
// 	const float dWestBalcony = fBox( p - vec3( -10.0f, 0.0f, 0.0f ), vec3( 4.0f, 0.1f, 48.0f ) );
// 	const float dBalconies = min( dEastBalcony, dWestBalcony );
// 	sceneDist = min( dBalconies, sceneDist );
// 	if ( sceneDist == dBalconies && dBalconies < raymarchEpsilon ) {
// 		hitPointColor = floorCielingColor;
// 		hitPointSurfaceType = DIFFUSE;
// 	}

// 	// store point value before applying repeat
// 	const vec3 pCache = p;
// 	pMirror( p.x, 0.0f );

// 	// compute bounding box for the rails on both sides, using the mirrored point
// 	const float dRailBounds = fBox( p - vec3( 7.0f, 1.625f, 0.0f ), vec3( 1.0f, 1.2f, 48.0f ) );

// 	// if railing bounding box is true
// 	float dRails;
// 	if ( dRailBounds < 0.0f ) {
// 		// railings - probably use some instancing on them, also want to use a bounding volume
// 		dRails = fCapsule( p, vec3( 7.0f, 2.4f, 48.0f ), vec3( 7.0f, 2.4f, -48.0f ), 0.3f );
// 		dRails = min( dRails, fCapsule( p, vec3( 7.0f, 0.6f, 48.0f ), vec3( 7.0f, 0.6f, -48.0f ), 0.1f ) );
// 		dRails = min( dRails, fCapsule( p, vec3( 7.0f, 1.1f, 48.0f ), vec3( 7.0f, 1.1f, -48.0f ), 0.1f ) );
// 		dRails = min( dRails, fCapsule( p, vec3( 7.0f, 1.6f, 48.0f ), vec3( 7.0f, 1.6f, -48.0f ), 0.1f ) );
// 		sceneDist = min( dRails, sceneDist );
// 		if ( sceneDist == dRails && dRails <= raymarchEpsilon ) {
// 			hitPointColor = vec3( 0.618f );
// 			hitPointSurfaceType = METALLIC;
// 		}
// 	} // end railing bounding box

// 	// revert to original point value
// 	p = pCache;

// 	pMod1( p.x, 14.0f );
// 	p.z += 2.0f;
// 	pModMirror1( p.z, 4.0f );
// 	float dArches = fBox( p - vec3( 0.0f, 4.9f, 0.0f ), vec3( 10.0f, 5.0f, 5.0f ) );
// 	dArches = fOpDifferenceRound( dArches, fRoundedBox( p - vec3( 0.0f, 0.0f, 3.0f ), vec3( 10.0f, 4.5f, 1.0f ), 3.0f ), 0.2f );
// 	dArches = fOpDifferenceRound( dArches, fRoundedBox( p, vec3( 3.0f, 4.5f, 10.0f ), 3.0f ), 0.2f );

// 	// if railing bounding box is true
// 	if ( dRailBounds < 0.0f ) {
// 		dArches = fOpDifferenceRound( dArches, dRails - 0.05f, 0.1f );
// 	} // end railing bounding box

// 	sceneDist = min( dArches, sceneDist );
// 	if ( sceneDist == dArches && dArches < raymarchEpsilon ) {
// 		hitPointColor = floorCielingColor;
// 		hitPointSurfaceType = DIFFUSE;
// 	}

// 	p = pCache;

// 	// the bar lights are the primary source of light in the scene
// 	const float dCenterLightBar = fBox( p - vec3( 0.0f, 7.4f, 0.0f ), vec3( 1.0f, 0.1f, 48.0f ) );
// 	sceneDist = min( dCenterLightBar, sceneDist );
// 	if ( sceneDist == dCenterLightBar && dCenterLightBar <=raymarchEpsilon ) {
// 		hitPointColor = 0.6f * GetColorForTemperature( 6500.0f );
// 		hitPointSurfaceType = EMISSIVE;
// 	}

// 	const vec3 coolColor = 0.8f * pow( GetColorForTemperature( 1000000.0f ), vec3( 3.0f ) );
// 	const vec3 warmColor = 0.8f * pow( GetColorForTemperature( 1000.0f ), vec3( 1.2f ) );

// 	const float dSideLightBar1 = fBox( p - vec3( 7.5f, -0.4f, 0.0f ), vec3( 0.618f, 0.05f, 48.0f ) );
// 	sceneDist = min( dSideLightBar1, sceneDist );
// 	if ( sceneDist == dSideLightBar1 && dSideLightBar1 <= raymarchEpsilon ) {
// 		hitPointColor = coolColor;
// 		hitPointSurfaceType = EMISSIVE;
// 	}

// 	const float dSideLightBar2 = fBox( p - vec3( -7.5f, -0.4f, 0.0f ), vec3( 0.618f, 0.05f, 48.0f ) );
// 	sceneDist = min( dSideLightBar2, sceneDist );
// 	if ( sceneDist == dSideLightBar2 && dSideLightBar2 <= raymarchEpsilon ) {
// 		hitPointColor = warmColor;
// 		hitPointSurfaceType = EMISSIVE;
// 	}

// 	return sceneDist;
// }

// ==============================================================================================
// ==============================================================================================

#define NOHIT		0
#define EMISSIVE	1
#define DIFFUSE		2
#define METALLIC	3

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

	const vec3 pCache = p;
	const vec3 floorCielingColor = vec3( 0.9f );

	const float dFloor = fPlane( p, vec3( 0.0f, 1.0f, 0.0f ), 4.0f );
	sceneDist = min( dFloor, sceneDist );
	if ( sceneDist == dFloor && dFloor <= raymarchEpsilon ) {
		hitPointColor = floorCielingColor;
		hitPointSurfaceType = DIFFUSE;
	}

	// hexagonal symmetry
	pModPolar( p.xz, 6.0f );

	const float dWall = fPlane( p, vec3( -1.0f, 0.0f, 0.0f ), 35.0f );
	sceneDist = min( dWall, sceneDist );
	if ( sceneDist == dWall && dWall <= raymarchEpsilon ) {
		hitPointColor = floorCielingColor * 0.7f;
		hitPointSurfaceType = DIFFUSE;
	}

	const float dUpperWall = fPlane( p, vec3( -1.0f, -1.0f, 0.0f ), 40.0f );
	sceneDist = min( dUpperWall, sceneDist );
	if ( sceneDist == dUpperWall && dUpperWall <= raymarchEpsilon ) {
		hitPointColor = vec3( 1.0f );
		hitPointSurfaceType = DIFFUSE;
	}

	const float dLightBars = fBoxCheap( p - vec3( 0.0f, 10.0f, 0.0f ), vec3( 10.0f, 0.1f, 1.0f ) );
	sceneDist = min( dLightBars, sceneDist );
	if ( sceneDist == dLightBars && dLightBars <= raymarchEpsilon ) {
		hitPointColor = GetColorForTemperature( 4800.0f );
		hitPointSurfaceType = EMISSIVE;
	}

	const float dLightBarHousing = fBoxCheap( p - vec3( 0.0f, 10.5f, 0.0f ), vec3( 11.0f, 0.4f, 1.1f ) );
	sceneDist = min( dLightBarHousing, sceneDist );
	if ( sceneDist == dLightBarHousing && dLightBarHousing <= raymarchEpsilon ) {
		hitPointColor = vec3( 0.618f );
		hitPointSurfaceType = DIFFUSE;
	}


	const float dFractal = deFractal( pCache );
	sceneDist = min( dFractal, sceneDist );
	if ( sceneDist == dFractal && dFractal <= raymarchEpsilon ) {
		hitPointColor = vec3( 0.9f, 0.5f, 0.3f );
		hitPointSurfaceType = METALLIC;
	}

	// const float dRails = min( min( fTorus( p, 0.2f, 28.0f ), fTorus( p + vec3( 0.0f, 1.0f, 0.0f ), 0.1f, 28.0f ) ), fTorus( p + vec3( 0.0f, 2.0f, 0.0f ), 0.1f, 28.0f ) );
	// sceneDist = min( dRails, sceneDist );
	// if ( sceneDist == dRails && dRails <= raymarchEpsilon ) {
	// 	hitPointColor = vec3( 0.7f, 0.4f, 0.2f );
	// 	hitPointSurfaceType = METALLIC;
	// }

	return sceneDist;
}

// ==============================================================================================
// ray scattering functions

vec3 HenyeyGreensteinSampleSphere ( const vec3 n, const float g ) {
	float t = ( 1.0f - g * g ) / ( 1.0f - g + 2.0f * g * NormalizedRandomFloat() );
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
	if ( NormalizedRandomFloat() < 0.005f ) {
		return normalize( n + 0.4f * RandomUnitVector() );
	}
}

// ==============================================================================================
// raymarches to the next hit

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

// ==============================================================================================

vec3 ColorSample ( const vec2 uvIn ) {
	const float aspectRatio = float( imageSize( accumulatorColor ).x ) / float( imageSize( accumulatorColor ).y );

	// compute initial ray origin, direction
	vec3 rayOrigin_initial = viewerPosition;
	vec3 rayDirection_initial = normalize( aspectRatio * uvIn.x * basisX + uvIn.y * basisY + ( 1.0f / FoV ) * basisZ );

	// thin lens adjustment
	vec3 focuspoint = rayOrigin_initial + ( ( rayDirection_initial * thinLensFocusDistance ) / dot( rayDirection_initial, basisZ ) );
	vec2 diskOffset = thinLensJitterRadius * RandomInUnitDisk();
	rayOrigin_initial += diskOffset.x * basisX + diskOffset.y * basisY;
	rayDirection_initial = normalize( focuspoint - rayOrigin_initial );

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
		const int hitPointSurfaceType_cache = hitPointSurfaceType;
		const vec3 hitPointColor_cache = hitPointColor;

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
		const vec3 reflectedVector = reflect( rayDirectionPrevious, hitNormal );
		const vec3 randomVectorDiffuse = normalize( ( 1.0f + raymarchEpsilon ) * hitNormal + RandomUnitVector() );
		const vec3 randomVectorSpecular = normalize( ( 1.0f + raymarchEpsilon ) * hitNormal + mix( reflectedVector, RandomUnitVector(), 0.1f ) );

		// add a check for points that are not within epsilon? just ran out the steps?
			// this is much less likely with a 0.9 understep, rather than 0.618

		if ( dHit >= raymarchMaxDistance ) {

			// ray escaped - break out of loop, since you will not bounce
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

			case METALLIC:
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
		const float sampleCount = oldColor.a + 1.0f;

		// new values - color is currently placeholder
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
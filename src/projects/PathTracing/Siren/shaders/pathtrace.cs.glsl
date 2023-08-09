// #include "hg_sdf.glsl" // SDF modeling functions

// #define AA 1 // AA value of 2 means each sample is actually 2*2 = 4 offset samples, slows things way down

// // core rendering stuff
// uniform ivec2	tileOffset;			// tile renderer offset for the current tile
// uniform ivec2	noiseOffset;		// jitters the noise sample read locations
// uniform int		maxSteps;			// max steps to hit
// uniform int		maxBounces;			// number of pathtrace bounces
// uniform float	maxDistance;		// maximum ray travel
// uniform float 	understep;			// scale factor on distance, when added as raymarch step
// uniform float	epsilon;			// how close is considered a surface hit
// uniform int		normalMethod;		// selector for normal computation method
// uniform float	focusDistance;		// for thin lens approx
// uniform float	thinLensIntensity;	// scalar on the thin lens DoF effect
// uniform float	FoV;				// field of view
// uniform float	exposure;			// exposure adjustment
// uniform vec3		viewerPosition;		// position of the viewer
// uniform vec3		basisX;				// x basis vector
// uniform vec3		basisY;				// y basis vector
// uniform vec3		basisZ;				// z basis vector
// uniform int		wangSeed;			// integer value used for seeding the wang hash rng
// uniform int		modeSelect;			// do we do a pathtrace sample, or just the preview

// // render modes
// #define PATHTRACE		0
// #define PREVIEW_DIFFUSE	1
// #define PREVIEW_NORMAL	2
// #define PREVIEW_DEPTH	3
// #define PREVIEW_SHADED	4 // TODO: some basic phong + AO... tbd

// // lens parameters
// uniform float lensScaleFactor;		// scales the lens DE
// uniform float lensRadius1;			// radius of the sphere for the first side
// uniform float lensRadius2;			// radius of the sphere for the second side
// uniform float lensThickness;		// offset between the two spheres
// uniform float lensRotate;			// rotating the displacement offset betwee spheres
// uniform float lensIoR;				// index of refraction for the lens
// uniform bool showLens;				// whether or not to show lens sdf

// // scene parameters
// uniform vec3 redWallColor;
// uniform vec3 greenWallColor;
// uniform vec3 whiteWallColor;
// uniform vec3 floorCielingColor;
// uniform vec3 metallicDiffuse;

// // global state
// 	// requires manual management of geo, to ensure that the lens material does not intersect with itself
// bool enteringRefractive = false; // multiply by the lens distance estimate, to invert when inside a refractive object
// float sampleCount = 0.0f;

// #include "BRDFUtils.glsl" // Namless's BRDF code

// #define NOHIT 0
// #define DIFFUSE 1
// #define PERFECTREFLECT 2
// #define METALLIC 3
// #define EMISSIVE 4
// #define REFRACTIVE 5
// #define GGX 6

// // eventually, probably define a list of materials, and index into that - that will allow for
// 	// e.g. refractive materials of multiple different indices of refraction

// // fake AO, computed from SDF
// float calcAO ( in vec3 position, in vec3 normal ) {
// 	float occ = 0.0f;
// 	float sca = 1.0f;
// 	for( int i = 0; i < 5; i++ ) {
// 		float h = 0.001f + 0.15f * float( i ) / 4.0f;
// 		float d = de( position + h * normal );
// 		occ += ( h - d ) * sca;
// 		sca *= 0.95f;
// 	}
// 	return clamp( 1.0f - 1.5f * occ, 0.0f, 1.0f );
// }

// // normalized gradient of the SDF - 3 different methods
// vec3 normal ( vec3 p ) {
// 	vec2 e;
// 	switch( normalMethod ) {
// 		case 0: // tetrahedron version, unknown original source - 4 DE evaluations
// 			e = vec2( 1.0f, -1.0f ) * epsilon;
// 			return normalize( e.xyy * de( p + e.xyy ) + e.yyx * de( p + e.yyx ) + e.yxy * de( p + e.yxy ) + e.xxx * de( p + e.xxx ) );
// 			break;

// 		case 1: // from iq = more efficient, 4 DE evaluations
// 			e = vec2( epsilon, 0.0f );
// 			return normalize( vec3( de( p ) ) - vec3( de( p - e.xyy ), de( p - e.yxy ), de( p - e.yyx ) ) );
// 			break;

// 		case 2: // from iq - less efficient, 6 DE evaluations
// 			e = vec2( epsilon, 0.0f );
// 			return normalize( vec3( de( p + e.xyy ) - de( p - e.xyy ), de( p + e.yxy ) - de( p - e.yxy ), de( p + e.yyx ) - de( p - e.yyx ) ) );
// 			break;

// 		default:
// 			break;
// 	}
// }

// // there's definitely a better way to do this, instead of two separate functions - some preprocessor fuckery? tbd
// vec3 lensNormal ( vec3 p ) {
// 	vec2 e;
// 	switch( normalMethod ) {
// 		case 0: // tetrahedron version, unknown original source - 4 DE evaluations
// 			e = vec2( 1.0f, -1.0f ) * epsilon;
// 			return normalize( e.xyy * deLens( p + e.xyy ) + e.yyx * deLens( p + e.yyx ) + e.yxy * deLens( p + e.yxy ) + e.xxx * deLens( p + e.xxx ) );
// 			break;

// 		case 1: // from iq = more efficient, 4 DE evaluations
// 			e = vec2( epsilon, 0.0f );
// 			return normalize( vec3( deLens( p ) ) - vec3( deLens( p - e.xyy ), deLens( p - e.yxy ), deLens( p - e.yyx ) ) );
// 			break;

// 		case 2: // from iq - less efficient, 6 DE evaluations
// 			e = vec2( epsilon, 0.0f );
// 			return normalize( vec3( deLens( p + e.xyy ) - deLens( p - e.xyy ), deLens( p + e.yxy ) - deLens( p - e.yxy ), deLens( p + e.yyx ) - deLens( p - e.yyx ) ) );
// 			break;

// 		default:
// 			break;
// 	}
// }

// float reflectance ( float cosTheta, float IoR ) {
// 	// Use Schlick's approximation for reflectance
// 	float r0 = ( 1.0f - IoR ) / ( 1.0f + IoR );
// 	r0 = r0 * r0;
// 	return r0 + ( 1.0f - r0 ) * pow( ( 1.0f - cosTheta ), 5.0f );
// }

// // raymarches to the next hit
// float raymarch ( vec3 origin, vec3 direction ) {
// 	float dQuery = 0.0f;
// 	float dTotal = 0.0f;
// 	for ( int steps = 0; steps < maxSteps; steps++ ) {
// 		vec3 pQuery = origin + dTotal * direction;
// 		dQuery = de( pQuery );
// 		dTotal += dQuery * understep;
// 		if ( dTotal > maxDistance || abs( dQuery ) < epsilon ) {
// 			break;
// 		}
// 		// // certain chance to scatter in a random direction, per step - one of Nameless' methods for fog
// 		// if ( normalizedRandomFloat() < 0.005f ) { // massive slowdown doing this
// 		// 	direction = normalize( direction + 0.4f * randomUnitVector() );
// 		// }
// 	}
// 	return dTotal;
// }

// ivec2 location = ivec2( 0, 0 );	// 2d location, pixel coords
// vec3 colorSample ( vec3 rayOrigin_in, vec3 rayDirection_in ) {

// 	vec3 rayOrigin = rayOrigin_in, previousRayOrigin;
// 	vec3 rayDirection = rayDirection_in, previousRayDirection;
// 	vec3 finalColor = vec3( 0.0f );
// 	vec3 throughput = vec3( 1.0f );

// 	// bump origin up by unit vector - creates fuzzy / soft section plane
// 	// rayOrigin += rayDirection * ( 0.9f + 0.1f * blueNoiseReference( location ).x );

// 	// debug output
// 	if ( modeSelect != PATHTRACE ) {
// 		const float rayDistance = raymarch( rayOrigin, rayDirection );
// 		const vec3 pHit = rayOrigin + rayDistance * rayDirection;
// 		const vec3 hitpointNormal = normal( pHit );
// 		const vec3 hitpointDepth = vec3( 1.0f / rayDistance );
// 		if ( de( pHit ) < epsilon ) {
// 			switch ( modeSelect ) {
// 				case PREVIEW_DIFFUSE: return hitpointColor; break;
// 				case PREVIEW_NORMAL: return hitpointNormal; break;
// 				case PREVIEW_DEPTH: return hitpointDepth; break;
// 				case PREVIEW_SHADED: return hitpointColor * ( 1.0f / calcAO( pHit, hitpointNormal ) ); break;
// 			}
// 		} else {
// 			return vec3( 0.0f );
// 		}
// 	}

// 	// loop to max bounces
// 	for( int bounce = 0; bounce < maxBounces; bounce++ ) {
// 		float dResult = raymarch( rayOrigin, rayDirection );
// 		int hitpointSurfaceType_cache = hitpointSurfaceType;
// 		vec3 hitpointColor_cache = hitpointColor;

// 		// cache previous values of rayOrigin, rayDirection, and get new hit position
// 		previousRayOrigin = rayOrigin;
// 		previousRayDirection = rayDirection;
// 		rayOrigin = rayOrigin + dResult * rayDirection;

// 		// surface normal at the new hit position
// 		vec3 hitNormal = normal( rayOrigin );

// 		// bump rayOrigin along the normal to prevent false positive hit on next bounce
// 			// now you are at least epsilon distance from the surface, so you won't immediately hit
// 		if ( hitpointSurfaceType_cache != REFRACTIVE ) {
// 			rayOrigin += 2.0f * epsilon * hitNormal;
// 		}

// 	// these are mixed per-material
// 		// construct new rayDirection vector, diffuse reflection off the surface
// 		vec3 reflectedVector = reflect( previousRayDirection, hitNormal );

// 		vec3 randomVectorDiffuse = normalize( ( 1.0f + epsilon ) * hitNormal + randomUnitVector() );
// 		vec3 randomVectorSpecular = normalize( ( 1.0f + epsilon ) * hitNormal + mix( reflectedVector, randomUnitVector(), 0.1f ) );



// 		// currently just implementing diffuse and emissive behavior
// 			// eventually add different ray behaviors for each material here
// 		switch ( hitpointSurfaceType_cache ) {

// 			case EMISSIVE:
// 				finalColor += throughput * hitpointColor;
// 				break;

// 			case DIFFUSE:
// 				rayDirection = randomVectorDiffuse;
// 				throughput *= hitpointColor_cache; // attenuate throughput by surface albedo
// 				break;

// 			case METALLIC:
// 				rayDirection = mix( randomVectorDiffuse, randomVectorSpecular, 0.7f );
// 				throughput *= hitpointColor_cache;
// 				break;

// 			case REFRACTIVE: // ray refracts, instead of bouncing
// 				// bump by the appropriate amount
// 				vec3 lensNorm = ( enteringRefractive ? 1.0f : -1.0f ) * lensNormal( rayOrigin );
// 				rayOrigin -= 2.0f * epsilon * lensNorm;

// 				// entering or leaving
// 				// float IoR = enteringRefractive ? lensIoR : 1.0f / lensIoR;
// 				float IoR = enteringRefractive ? ( 1.0f / lensIoR ) : lensIoR;
// 				float cosTheta = min( dot( -normalize( rayDirection ), lensNorm ), 1.0f );
// 				float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );

// 				// accounting for TIR effects
// 				bool cannotRefract = ( IoR * sinTheta ) > 1.0f;
// 				if ( cannotRefract || reflectance( cosTheta, IoR ) > normalizedRandomFloat() ) {
// 					rayDirection = reflect( normalize( rayDirection ), lensNorm );
// 				} else {
// 					rayDirection = refract( normalize( rayDirection ), lensNorm, IoR );
// 				}
// 				break;

// 			case GGX:
// 				float specularProbability = 0.9f; // could set by fresnel
// 				float roughnessValue = 0.01f;

// 				rayDirection = randomVectorDiffuse;
// 				if( normalizedRandomFloat() < specularProbability ){
// 					rayDirection = ggx_S( reflect( previousRayDirection, hitNormal ), roughnessValue );
// 				}

// 				vec3 h = normalize( -previousRayDirection + rayDirection );
// 				float D = ggx_D( max( dot( reflect( previousRayDirection, hitNormal ),rayDirection ), 0.0f ), roughnessValue );
// 				float G = cookTorranceG( hitNormal, h, -previousRayDirection,rayDirection );
// 				vec3 F = Schlick( hitpointColor, max( dot( -previousRayDirection, hitNormal ), 0.0f ) );
// 				vec3 specular = ( D * G * F ) / max( 4.0f * max( dot( rayDirection, hitNormal ), 0.6f ) * max( dot( -previousRayDirection, hitNormal ), 0.0f ), 0.001f );

// 				vec3 brdf = hitpointColor / PI;
// 				float pdf = 1.0f / ( 2.0f * PI );
// 				brdf = mix( brdf, specular, specularProbability );
// 				pdf = mix( pdf, ggx_pdf( max( dot( reflect( previousRayDirection, hitNormal ), rayDirection ), 0.0f ), roughnessValue ), specularProbability );

// 				brdf *= 1.0f + ( 2.0f * specularProbability * max( dot( rayDirection, hitNormal ), 0.0f ) );
// 				pdf = max( pdf, 0.0001f );

// 				throughput *= brdf * max( dot( rayDirection, hitNormal ), 0.0f ) / pdf;
// 				break;

// 			default:
// 				break;
// 		}

// 		// if ( hitpointSurfaceType_cache != REFRACTIVE ) {
// 		// 	// russian roulette termination - chance for ray to quit early
// 		// 	float maxChannel = max( throughput.r, max( throughput.g, throughput.b ) );
// 		// 	if ( normalizedRandomFloat() > maxChannel ) { break; }
// 		// 	// russian roulette compensation term
// 		// 	throughput *= 1.0f / maxChannel;
// 		// }
// 	}
// 	return finalColor;
// }

// #define BLUE
// vec2 getRandomOffset ( int n ) {
// 	// weyl sequence from http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/ and https://www.shadertoy.com/view/4dtBWH
// 	#ifdef UNIFORM
// 		return fract( vec2( 0.0f ) + vec2( n * 12664745, n * 9560333 ) / exp2( 24.0f ) );	// integer mul to avoid round-off
// 	#endif
// 	#ifdef UNIFORM2
// 		return fract( vec2( 0.0f ) + float( n ) * vec2( 0.754877669f, 0.569840296f ) );
// 	#endif
// 	#ifdef RANDOM // wang hash random offsets
// 		return vec2( normalizedRandomFloat(), normalizedRandomFloat() );
// 	#endif
// 	#ifdef BLUE
// 		return blueNoiseReference( ivec2( gl_GlobalInvocationID.xy ) ).xy;
// 	#endif
// }

// void storeNormalAndDepth ( vec3 normal, float depth ) {
// 	// blend with history and imageStore
// 	vec4 prevResult = imageLoad( accumulatorNormalsAndDepth, location );
// 	vec4 blendResult = mix( prevResult, vec4( normal, depth ), 1.0f / sampleCount );
// 	imageStore( accumulatorNormalsAndDepth, location, blendResult );
// }

// vec3 pathtraceSample ( ivec2 location, int n ) {
// 	vec3  cResult = vec3( 0.0f );
// 	vec3  nResult = vec3( 0.0f );
// 	float dResult = 0.0f;
// 	const float aspectRatio = float( imageSize( accumulatorColor ).x ) / float( imageSize( accumulatorColor ).y );

// #if AA != 1
// 	// at AA = 2, this is 4 samples per invocation
// 	const float normalizeTerm = float( AA * AA );
// 	for ( int x = 0; x < AA; x++ ) {
// 		for ( int y = 0; y < AA; y++ ) {
// #endif

// 			// pixel offset + mapped position
// 			// vec2 offset = vec2( x + normalizedRandomFloat(), y + normalizedRandomFloat() ) / float( AA ) - 0.5; // previous method
// 			vec2 subpixelOffset  = getRandomOffset( n );
// 			vec2 halfScreenCoord = vec2( imageSize( accumulatorColor ) / 2.0f );
// 			vec2 mappedPosition  = ( vec2( location + subpixelOffset ) - halfScreenCoord ) / halfScreenCoord;

// 			vec3 rayDirection = normalize( aspectRatio * mappedPosition.x * basisX + mappedPosition.y * basisY + ( 1.0f / FoV ) * basisZ );
// 			vec3 rayOrigin    = viewerPosition;

// 			// thin lens DoF - adjust view vectors to converge at focusDistance
// 				// this is a small adjustment to the ray origin and direction - not working correctly - need to revist this
// 			vec3 focuspoint = rayOrigin + ( ( rayDirection * focusDistance ) / dot( rayDirection, basisZ ) );
// 			vec2 diskOffset = thinLensIntensity * randomInUnitDisk();
// 			rayOrigin       += diskOffset.x * basisX + diskOffset.y * basisY;
// 			rayDirection    = normalize( focuspoint - rayOrigin );

// 			// get depth and normals - think about special handling for refractive hits, maybe consider total distance traveled after all bounces?
// 			float distanceToFirstHit = raymarch( rayOrigin, rayDirection ); // may need to use more raymarch steps / decrease understep, creates some artifacts
// 			storeNormalAndDepth( normal( rayOrigin + distanceToFirstHit * rayDirection ), distanceToFirstHit );

// 			// get the result for a ray
// 			cResult += colorSample( rayOrigin, rayDirection );

// #if AA != 1
// 		}
// 	}
// 	// multisample compensation req'd
// 	cResult /= normalizeTerm;
// #endif

// 	return cResult * exposure;
// }

// void main () {
// 	location = ivec2( gl_GlobalInvocationID.xy ) + tileOffset;
// 	if ( !boundsCheck( location ) ) return; // abort on out of bounds

// 	seed = location.x * 1973 + location.y * 9277 + wangSeed;

// 	switch ( modeSelect ) {
// 		case PATHTRACE:
// 			vec4 prevResult = imageLoad( accumulatorColor, location );
// 			sampleCount = prevResult.a + 1.0f;
// 			vec3 blendResult = mix( prevResult.rgb, pathtraceSample( location, int( sampleCount ) ), 1.0f / sampleCount );
// 			imageStore( accumulatorColor, location, vec4( blendResult, sampleCount ) );
// 			break;

// 		case PREVIEW_DIFFUSE:
// 		case PREVIEW_NORMAL:
// 		case PREVIEW_DEPTH:
// 		case PREVIEW_SHADED:
// 			location = ivec2( gl_GlobalInvocationID.xy );
// 			imageStore( accumulatorColor, location, vec4( pathtraceSample( location, 0 ), 1.0f ) );
// 			break;

// 		default:
// 			return;
// 			break;
// 	}
// }

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

mat3 rotate3D ( float angle, vec3 axis ) {
	vec3 a = normalize( axis );
	float s = sin( angle );
	float c = cos( angle );
	float r = 1.0f - c;
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
uint wangHash () {
	seed = uint( seed ^ uint( 61 ) ) ^ uint( seed >> uint( 16 ) );
	seed *= uint( 9 );
	seed = seed ^ ( seed >> 4 );
	seed *= uint( 0x27d4eb2d );
	seed = seed ^ ( seed >> 15 );
	return seed;
}

float normalizedRandomFloat () {
	return float( wangHash() ) / 4294967296.0f;
}

vec3 randomUnitVector () {
	float z = normalizedRandomFloat() * 2.0f - 1.0f;
	float a = normalizedRandomFloat() * 2.0f * PI;
	float r = sqrt( 1.0f - z * z );
	float x = r * cos( a );
	float y = r * sin( a );
	return vec3( x, y, z );
}

vec2 randomInUnitDisk () {
	return randomUnitVector().xy;
}

uvec4 blueNoiseReference ( ivec2 location ) {
	location += noiseOffset;
	location.x = location.x % imageSize( blueNoise ).x;
	location.y = location.y % imageSize( blueNoise ).y;
	return imageLoad( blueNoise, location );
}

bool boundsCheck ( ivec2 loc ) {
	// used to abort off-image samples
	ivec2 bounds = ivec2( imageSize( accumulatorColor ) ).xy;
	return ( loc.x < bounds.x && loc.y < bounds.y );
}

void main () {
	uvec2 location = gl_GlobalInvocationID.xy + tileOffset.xy;

	// wang hash seeded uniquely for every pixel
	seed = wangSeed + 42069 * location.x + 451 * location.y;

	// uint result = blueNoiseReference( ivec2( location ) ).r;
	// imageStore( accumulatorColor, ivec2( location ), vec4( vec3( result / 255.0f ), 1.0f ) );

	// imageStore( accumulatorColor, ivec2( location ), vec4( normalizedRandomFloat(), normalizedRandomFloat(), normalizedRandomFloat(), 1.0f ) );

	vec2 uv = ( vec2( location ) + vec2( 0.5f ) ) / vec2( imageSize( accumulatorColor ).xy );
	imageStore( accumulatorColor, ivec2( location ), vec4( uv.xy, 0.0f, 1.0f ) );
}
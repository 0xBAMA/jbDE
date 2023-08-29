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
		vec2 diskOffset = thinLensJitterRadius * RandomInUnitDisk();
		r.origin += diskOffset.x * basisX + diskOffset.y * basisY;
		r.direction = normalize( focuspoint - r.origin );
	}

	return r;
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
	#define V vec2(.7,-.7)
	#define G(p)dot(p,V)
	float i=0.,g=0.,e=1.;
	float t = 0.34; // change to see different behavior
	for(int j=0;j++<8;){
		p=abs(Rotate3D(0.34,vec3(1,-3,5))*p*2.)-1.,
		p.xz-=(G(p.xz)-sqrt(G(p.xz)*G(p.xz)+.05))*V;
	}
	return length(p.xz)/3e2;
	#undef V
	#undef G
}

mat2 rot2(in float a){ float c = cos(a), s = sin(a); return mat2(c, s, -s, c); }
float deOrganic3 ( vec3 p ) {
	float d = 1e5;
	const int n = 3;
	const float fn = float(n);
	for(int i = 0; i < n; i++){
		vec3 q = p;
		float a = float(i)*fn*2.422; //*6.283/fn
		a *= a;
		q.z += float(i)*float(i)*1.67; //*3./fn
		q.xy *= rot2(a);
		float b = (length(length(sin(q.xy) + cos(q.yz))) - .15);
		float f = max(0., 1. - abs(b - d));
		d = min(d, b) - .25*f*f;
	}
	return d;
}

// tree shape
mat2 rotate2D( float r ) {
	return mat2( cos( r ), sin( r ), -sin( r ), cos( r ) );
}
float deTree ( vec3 p ) {
	float d, a;
	d = a = 1.0f;
	for ( int j = 0; j++ < 16; )
		p.xz = abs( p.xz ) * rotate2D( PI / 3.0f ),
		d = min( d, max( length( p.zx ) - 0.3f, p.y - 0.4f ) / a ),
		p.yx *= rotate2D( 0.5f ),
		p.y -= 3.0f,
		p *= 1.6f,
		a *= 1.6f;
	return d;
}

float deJenga ( vec3 p ){
	vec3 P=p, Q, b=vec3( 4, 2.8, 15 );
	float i, d=1., a;
	Q = mod( P, b ) - b * 0.5;
	d = P.z - 6.0;
	a = 1.3;
	for( int j = 0; j++ < 17; )
		d = min( d, length( max( abs( Q ) - b.zyy / 13.0, 0.0 ) ) / a ),
		Q = vec3( Q.y, abs( Q.x ) - 1.0, Q.z + 0.3 ) * 1.4,
		a *= 1.4;
	return d;
}

vec3 triangles ( vec3 p ) {
	const float sqrt3 = sqrt( 3.0 );
	float zm = 1.;
	p.x = p.x-sqrt3*(p.y+.5)/3.;
	p = vec3( mod( p.x + sqrt3 / 2.0, sqrt3 ) - sqrt3 / 2.0, mod( p.y + 0.5, 1.5 ) - 0.5, mod( p.z + 0.5 * zm, zm ) - 0.5 * zm );
	p = vec3( p.x / sqrt3, ( p.y + 0.5 ) * 2.0 / 3.0 - 0.5, p.z );
	p = p.y > -p.x ? vec3( -p.y, -p.x , p.z ) : p;
	p = vec3( p.x * sqrt3, ( p.y + 0.5 ) * 3.0 / 2.0 - 0.5, p.z );
	return vec3( p.x + sqrt3 * ( p.y + 0.5 ) / 3.0, p.y , p.z );
}
float deFractal2 ( vec3 p ) {
	float scale = 1.0;
	float s = 1.0 / 3.0;
	for ( int i = 0; i < 10; i++ ) {
		p = triangles( p );
		float r2 = dot( p, p );
		float k = s / r2;
		p = p * k;
		scale=scale * k;
	}
	return 0.3 * length( p ) / scale - 0.001 / sqrt( scale );
}

#define rot(a) mat2(cos(a),sin(a),-sin(a),cos(a))
float deFractal(vec3 p){
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

float deStairs ( vec3 P ) {
	vec3 Q;
	float a, d = min( ( P.y - abs( fract( P.z ) - 0.5f ) ) * 0.7f, 1.5f - abs( P.x ) );
	for ( a = 2.0f; a < 6e2f; a += a )
		Q = P * a,
		Q.xz *= rotate2D( a ),
		d += abs( dot( sin( Q ), Q - Q + 1.0f ) ) / a / 7.0f;
	return d;
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
#define RAINBOW		4
#define MIRROR		5

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
	// const vec3 floorCielingColor = vec3( 0.9f );

	// const float dFloor = fPlane( p, vec3( 0.0f, 1.0f, 0.0f ), 4.0f );
	// sceneDist = min( dFloor, sceneDist );
	// if ( sceneDist == dFloor && dFloor <= raymarchEpsilon ) {
	// 	hitPointColor = floorCielingColor;
	// 	// hitPointSurfaceType = MIRROR;
	// 	hitPointSurfaceType = DIFFUSE;
	// }

	const float sinTime = sin( frameNumber / 1440.0f );
	const float cosTime = cos( frameNumber / 1440.0f );

	const float lightHeight = 7.0f + 2.0f * sinTime;
	const float lightDiameter = 7.0f;

	const float dLight = fCylinder( p - vec3( 0.0f, lightHeight, 0.0f ), lightDiameter, 0.1f );
	sceneDist = min( dLight, sceneDist );
	if ( sceneDist == dLight && dLight <= raymarchEpsilon ) {
		hitPointColor = vec3( 0.2f, 0.5f, 0.9f ).zyx;
		hitPointSurfaceType = EMISSIVE;
	}

	const float dLight2 = fCylinder( p - vec3( 0.0f, -lightHeight, 0.0f ), lightDiameter, 0.1f );
	sceneDist = min( dLight2, sceneDist );
	if ( sceneDist == dLight2 && dLight2 <= raymarchEpsilon ) {
		hitPointColor = vec3( 0.2f, 0.5f, 0.9f );
		hitPointSurfaceType = EMISSIVE;
	}

	const float dLightHousing = min(
		fCylinder( p - vec3( 0.0f, lightHeight + 0.1f, 0.0f ), lightDiameter + 0.3f, 0.15f ), // light 1
		fCylinder( p - vec3( 0.0f, -lightHeight - 0.1f, 0.0f ), lightDiameter + 0.3f, 0.15f ) // light 2
	);
	sceneDist = min( dLightHousing, sceneDist );
	if ( sceneDist == dLightHousing && dLightHousing <= raymarchEpsilon ) {
		hitPointColor = vec3( 0.618f );
		hitPointSurfaceType = DIFFUSE;
	}

	// const float dStairs = deStairs( ( pCache * 0.3f ) ) / 0.3f;
	// sceneDist = min( dStairs, sceneDist );
	// if ( sceneDist == dStairs && dStairs <= raymarchEpsilon ) {
	// 	hitPointColor = vec3( 0.18f );
	// 	hitPointSurfaceType = DIFFUSE;
	// }

	// const float dOrganic = deOrganic( ( pCache * 2.0f ) ) / 2.0f;
	// sceneDist = min( dOrganic, sceneDist );
	// if ( sceneDist == dOrganic && dOrganic <= raymarchEpsilon ) {
	// 	// hitPointColor = vec3( 0.618f );
	// 	hitPointSurfaceType = RAINBOW;
	// }

	// const float dFractal = deFractal( Rotate3D( PI / 2.0f, vec3( 0.0f, 0.0f, 1.0f ) ) * ( pCache * 0.2f ) ) / 0.2f;
	// const float dFractal = deJenga( pCache * 2.0f ) / 2.0f;
	// const float dFractal = deFractal2( pCache * 2.0f ) / 2.0f;
	// const float dFractal = deOrganic3( Rotate3D( PI / 2.0f, vec3( 1.0f, 1.0f, 1.0f ) ) * ( pCache * 1.0f ) ) / 1.0f;
	const float dFractal = fOpUnionRound( deOrganic3( p ), dLightHousing, 0.5f );
	sceneDist = min( dFractal, sceneDist );
	if ( sceneDist == dFractal && dFractal <= raymarchEpsilon ) {
		hitPointColor = vec3( 0.618f );
		// hitPointColor = vec3( 0.99f, 0.55f, 0.22f );
		// hitPointColor = vec3( 0.618f, 0.3f, 0.1f ).zyx;
		hitPointSurfaceType = ( NormalizedRandomFloat() < 0.2f ) ? MIRROR : DIFFUSE;
		// hitPointSurfaceType = MIRROR;
		// hitPointSurfaceType = DIFFUSE;
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

		// get the hit point
		float dHit = Raymarch( rayOrigin, rayDirection );

		// cache surface type, color, so it's not overwritten by calls to de() in normal vector calcs
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

			case RAINBOW:
				throughput *= ( hitNormal + 1.0f ) / 2.0f;
				rayDirection = randomVectorDiffuse;
				break;

			case MIRROR:
				// perfect mirror ( slight attenuation )
				throughput *= 0.95f;
				rayDirection = reflectedVector;
				break;
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
		const float hitDistance = Raymarch( r.origin, r.direction );
		const vec3 hitPoint = r.origin + hitDistance * r.direction;

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
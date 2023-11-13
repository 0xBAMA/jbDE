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

float NormalizedRandomFloat () {
	return float( wangHash() ) / 4294967296.0f;
}

const float pi = 3.14159265358979323846f;
vec3 RandomUnitVector () {
	float z = NormalizedRandomFloat() * 2.0f - 1.0f;
	float a = NormalizedRandomFloat() * 2.0f * pi;
	float r = sqrt( 1.0f - z * z );
	float x = r * cos( a );
	float y = r * sin( a );
	return vec3( x, y, z );
}

// note that this is not uniformly distributed, ring shaped
vec2 RandomInUnitDisk () {
	return RandomUnitVector().xy;
}

// size, corner rounding radius
float sdHexagon ( vec2 p, float s, float r ) {
	const vec3 k = vec3( -0.866025404f, 0.5f, 0.577350269f );
	p = abs( p );
	p -= 2.0f * min( dot( k.xy, p ), 0.0f ) * k.xy;
	p -= vec2( clamp( p.x, -k.z * s, k.z * s ), s );
	return length( p ) * sign( p.y ) - r;
}

vec2 RejectionSampleHexOffset () {
	vec2 cantidate;
	// generate points in a unit square, till one is inside a hexagon
	while ( sdHexagon( cantidate = vec2( NormalizedRandomFloat(), NormalizedRandomFloat() ) - vec2( 0.5f ), 0.3f, 0.0f ) > 0.0f );
	return 2.0f * cantidate;
}

// more of these uniform remapping functions
// https://www.shadertoy.com/view/7lGXWK

// uniform sampled hex from fadaaszhi on GP discord
vec2 UniformSampleHexagon ( vec2 u ) {
	u = 2.0f * u - 1.0f;
	float a = sqrt( 3.0f ) - sqrt( 3.0f - 2.25f * abs( u.x ) );
	return vec2( sign( u.x ) * a, u.y * ( 1.0f - a / sqrt( 3.0f ) ) );
}
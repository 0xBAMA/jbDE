#version 430

uniform sampler2D sphere;
uniform int inSeed;
uniform float time;
uniform float scale;
uniform float AR;
uniform mat3 trident;
uniform float frameWidth;

// moving lights state - bring this back? I would like to have the current state of the point lights shown visually
uniform int lightCount;

struct light_t {
	vec4 position;
	vec4 color;
};

layout( binding = 4, std430 ) buffer lightDataBuffer {
	light_t lightData[];
};

in float radius;
flat in float roughness;
in vec3 color;
in vec3 worldPosition;
in float height;
flat in int index; // flat means no interpolation across primitive

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

const float pi = 3.14159265358979323846f;
vec3 randomUnitVector () {
	float z = normalizedRandomFloat() * 2.0f - 1.0f;
	float a = normalizedRandomFloat() * 2.0f * pi;
	float r = sqrt( 1.0f - z * z );
	float x = r * cos( a );
	float y = r * sin( a );
	return vec3( x, y, z );
}

vec2 randomInUnitDisk () {
	return randomUnitVector().xy;
}

layout( depth_greater ) out float gl_FragDepth;
layout( location = 0 ) out vec4 glFragColor;
layout( location = 1 ) out vec4 normalResult;
layout( location = 2 ) out vec4 positionResult;

void main () {
	seed = uint( gl_FragCoord.x * 65901 ) + uint( gl_FragCoord.y * 10244 ) + uint( inSeed );

	vec2 sampleLocation = gl_PointCoord.xy;

	const int numSamples = 4;
	vec2 sampleLocations[ numSamples ];
	bool results[ numSamples ];
	int numFailed = 0;

	for ( int i = 0; i < numSamples; i++ ) {
		sampleLocations[ i ] = sampleLocation + ( vec2( normalizedRandomFloat(), normalizedRandomFloat() ) / radius );
		results[ i ] = true;
		if ( distance( sampleLocations[ i ], vec2( 0.5f ) ) >= 0.5f ) {
			results[ i ] = false;
			numFailed++;
		}
	}

	if ( ( float( numFailed ) / float( numSamples ) ) >= 0.5f ) discard;

	// vec4 tRead = texture( sphere, sampleLocation );
	// if ( tRead.x < 0.05f ) discard;

	float height = 0.0f;
	for ( int i = 0; i < numSamples; i++ ) {
		if ( results[ i ] ) {
			// height += 
		}
	}

	const mat3 inverseTrident = inverse( trident );
	vec3 normal = inverseTrident * vec3( 2.0f * ( sampleLocation - vec2( 0.5f ) ), -height );
	vec3 worldPosition_adjusted = worldPosition - normal * ( radius / frameWidth );

	gl_FragDepth = gl_FragCoord.z + ( ( radius / frameWidth ) * ( 1.0f - height ) );
	glFragColor = vec4( color, 1.0f );
	normalResult = vec4( normal, 1.0f );
	positionResult = vec4( worldPosition_adjusted, 1.0f );
}
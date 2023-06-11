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
layout( location = 3 ) out uvec4 materialID;

void main () {
	seed = uint( gl_FragCoord.x * 65901 ) + uint( gl_FragCoord.y * 10244 ) + uint( inSeed );

	vec2 sampleLocation = gl_PointCoord.xy +
		( vec2( normalizedRandomFloat(), normalizedRandomFloat() ) - vec2( 0.5f ) ) *
		vec2( 1.0f / ( radius - 1.0f ) );

	// analytic solution via pythagoras
	// vec2 centered = sampleLocation * 2.0f - vec2( 1.0f );
	vec2 centered = sampleLocation * 2.0f - vec2( 1.0f );
	float radiusSquared = dot( centered, centered );
	if ( radiusSquared > 1.0f ) discard;
	float tRead = sqrt( 1.0f - radiusSquared );

	const mat3 inverseTrident = inverse( trident );
	vec3 normal = inverseTrident * vec3( 2.0f * ( sampleLocation - vec2( 0.5f ) ), -tRead );
	vec3 worldPosition_adjusted = worldPosition - normal * ( radius / frameWidth );

	gl_FragDepth = gl_FragCoord.z + ( ( radius / frameWidth ) * ( 1.0f - tRead ) );
	glFragColor = vec4( color, 1.0f );
	normalResult = vec4( normal, 1.0f );
	positionResult = vec4( worldPosition_adjusted, 1.0f );
	materialID = uvec4( index + 1, 0, 0, 0 );
}
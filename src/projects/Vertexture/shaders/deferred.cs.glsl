#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform sampler2D depthTexture;
uniform sampler2D colorTexture;
uniform sampler2D normalTexture;
uniform sampler2D positionTexture;
uniform sampler2D idTexture;

// todo:
	// accumulate normals
	// accumulate position
	// accumulate depth
	// sample rejection
	// use uint id value from render target to replace color buffer + distinguishing between moving and static points
		// move the static point data to an SSBO and expose it here

uniform vec2 resolution;
uniform mat3 trident;

// from the old shaders
uniform int lightCount;

struct light_t {
	vec4 position;
	vec4 color;
};

layout( binding = 4, std430 ) buffer lightDataBuffer {
	light_t lightData[];
};

// random utilites
uniform int inSeed;
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

// SSAO Constants / Support Functions
uniform int AONumSamples;
uniform float AOIntensity;
uniform float AOScale;
uniform float AOBias;
uniform float AOSampleRadius;
uniform float AOMaxDistance;

float DoAO ( const vec2 texCoord, const vec2 uv, const vec3 p, const vec3 cNormal ) {
	vec3 diff = texture( positionTexture, texCoord + uv ).xyz - p;
	const float l = length( diff );
	const vec3 v = diff / l;
	const float d = l * AOScale;
	float AO = max( 0.0f, dot( cNormal, v ) - AOBias ) * ( 1.0f / ( 1.0f + d ) );
	AO *= smoothstep( AOMaxDistance, AOMaxDistance * 0.5f, l );
	return AO;
}

// adapted from https://www.shadertoy.com/view/Ms33WB by Reinder Nijhoff 2016
float SpiralAO ( vec2 uv, vec3 p, vec3 n, float rad ) {
	float goldenAngle = 2.4f;
	float ao = 0.0f;
	float inv = 1.0f / float( AONumSamples );
	float radius = 0.0f;

	float rotatePhase = normalizedRandomFloat() * 6.28f;
	float rStep = inv * rad;
	vec2 spiralUV = vec2( 0.0f );

	for ( int i = 0; i < AONumSamples; i++ ) {
		spiralUV.x = sin( rotatePhase );
		spiralUV.y = cos( rotatePhase );
		radius += rStep;
		ao += DoAO( uv, spiralUV * radius, p, n );
		rotatePhase += goldenAngle;
	}
	ao *= inv;
	return ao;
}

void main () {

	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	uvec3 blueNoise = uvec3( imageLoad( blueNoiseTexture, writeLoc % imageSize( blueNoiseTexture ) ).r * 0.0618f );
	seed = inSeed + 10612 * writeLoc.x + 385609 * writeLoc.y;

	vec2 sampleLocation = ( vec2( writeLoc ) + vec2( 0.5f ) ) / resolution;
	sampleLocation.y = 1.0f - sampleLocation.y;
	vec4 color = texture( colorTexture, sampleLocation );
	vec4 depth = texture( depthTexture, sampleLocation );
	vec4 normal = texture( normalTexture, sampleLocation );
	vec4 position = texture( positionTexture, sampleLocation );

	// geo has been written this frame
	if ( depth.r != 1.0f ) {

		// lighting
		const mat3 inverseTrident = inverse( trident );
		const vec3 eyePosition = vec3( 0.0f, 0.0f, -5.0f );
		const vec3 viewVector = inverseTrident * normalize( eyePosition - position.xyz );
		vec3 lightContribution = vec3( 0.0f );

		// pack into one of the remaining fields ( or add additional rendered target with material properties )
		const float roughness = 1.0f;

		for ( int i = 0; i < lightCount; i++ ) {

			// phong setup
			const vec3 lightLocation = lightData[ i ].position.xyz;
			const vec3 lightVector = normalize( lightLocation - position.xyz );
			const vec3 reflectedVector = normalize( reflect( lightVector, normal.xyz ) );

			// lighting calculation
			const float lightDot = dot( normal.xyz, lightVector );
			const float dLight = distance( position.xyz, lightLocation );

			const float distanceFactor = 1.0f / pow( dLight, 2.0f );
			const float diffuseContribution = distanceFactor * max( lightDot, 0.0f );
			const float specularContribution = distanceFactor * pow( max( dot( -reflectedVector, viewVector ), 0.0f ), roughness );

			lightContribution += lightData[ i ].color.rgb * ( diffuseContribution + ( ( lightDot > 0.0f ) ? specularContribution : 0.0f ) );
		}

		// calculate SSAO
		const float aoScalar = 1.0f - SpiralAO( sampleLocation, position.xyz, normalize( normal.xyz ), AOSampleRadius / depth.r ) * AOIntensity;

		vec3 outputValue = lightContribution * aoScalar * color.rgb;

		// int fcxmod2 = int( writeLoc.x ) % 2;
		// int fcymod3 = int( writeLoc.y ) % 3;
		// if ( ( fcymod3 == 0 ) || ( fcxmod2 == 0 ) ) {
		// 	outputValue = color.rgb;
		// }

		vec4 previous = imageLoad( accumulatorTexture, writeLoc );
		outputValue = mix( previous.xyz, outputValue.xyz, 0.01f );
		imageStore( accumulatorTexture, writeLoc, vec4( outputValue, 1.0f ) );

	} else {

		vec4 previous = imageLoad( accumulatorTexture, writeLoc );
		imageStore( accumulatorTexture, writeLoc, vec4( mix( previous.xyz, vec3( 0.0f ), 0.1f ), 1.0f ) );

		// streaks in the negative space, interesting
		// vec4 previous = imageLoad( accumulatorTexture, writeLoc - ivec2( 1, 2 ) );
		// imageStore( accumulatorTexture, writeLoc, vec4( previous.rgb, 1.0f ) );
	}

}
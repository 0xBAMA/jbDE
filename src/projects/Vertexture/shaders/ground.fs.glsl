#version 430

uniform float scale;
uniform mat3 trident;

// moving lights state
uniform int lightCount;
struct light_t {
	vec4 position;
	vec4 color;
};
layout( binding = 4, std430 ) buffer lightDataBuffer {
	light_t lightData[];
};

in vec3 color;
in vec3 normal;
in vec3 worldPosition;

out vec4 glFragColor;

void main () {

	// lighting
	const float maxDistFactor = 5.0f;
	const vec3 eyePosition = vec3( 0.0f, 0.0f, -5.0f );
	vec3 lightContribution = vec3( 0.0f );
	const vec3 viewVector = inverse( trident ) * normalize( eyePosition - worldPosition );

	for ( int i = 0; i < lightCount; i++ ) {
		// phong setup
		const vec3 lightLocation = lightData[ i ].position.xyz;
		const vec3 lightVector = normalize( lightLocation - worldPosition );
		const vec3 reflectedVector = normalize( reflect( lightVector, normal ) );

		const float dLight = distance( worldPosition, lightLocation );
		const float lightDot = dot( normal, lightVector );

		// lighting calculation
		const float distanceFactor = min( 1.0f / ( pow( dLight, 2.0f ) ), maxDistFactor );
		const float diffuseContribution = distanceFactor * max( lightDot, 0.0f );
		const float specularContribution = distanceFactor * pow( max( dot( reflectedVector, viewVector ), 0.0f ), 60.0f );

		lightContribution += lightData[ i ].color.rgb * ( diffuseContribution + ( ( lightDot > 0.0f ) ? specularContribution : 0.0f ) );
	}

	glFragColor = vec4( color * lightContribution, 1.0f );
}
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
in vec3 position;

out vec4 glFragColor;

void main () {

	// lighting
	const vec3 eyePosition = inverse( trident ) * vec3( 0.0f, 0.0f, -1.0f );
	vec3 lightContribution = vec3( 0.0f );
	for ( int i = 0; i < lightCount; i++ ) {
		const vec3 lightLocation = scale * trident * lightData[ i ].position.xyz;

		// phong setup
		const vec3 lightVector = normalize( position - lightLocation );
		const vec3 viewVector = normalize( position - eyePosition );
		const vec3 reflectedVector = normalize( reflect( lightVector, normal ) );
		const float distanceFactor = min( 1.0f / ( pow( distance( position, lightLocation ) / scale, 2.0f ) ), 3.0f );

		// lighting calculation
		const float lightDot = dot( normal, lightVector );
		const float diffuseContribution = distanceFactor * max( lightDot, 0.0f );
		const float specularContribution = distanceFactor * pow( max( dot( reflectedVector, viewVector ), 0.0f ), 60.0f );

		lightContribution += lightData[ i ].color.rgb * ( diffuseContribution + ( ( lightDot > 0.0f ) ? specularContribution : 0.0f ) );
	}

	glFragColor = vec4( color * lightContribution, 1.0f );
}
#version 430

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
in vec3 position;
in mat3 rot;

out vec4 glFragColor;

void main () {

	// lighting calculations
	float nearestDistance = 100000.0f;
	vec3 lightColor = vec3( 0.0f );
	for ( int i = 0; i < lightCount; i++ ) {
		const float lightDist = distance( rot * lightData[ i ].position.xyz, position );
		nearestDistance = min( nearestDistance, lightDist );
		if ( nearestDistance == lightDist ) {
			lightColor = lightData[ i ].color.rgb;
		}
	}

	glFragColor = vec4( color * lightColor * ( 1.0f / nearestDistance ), 1.0f );
}
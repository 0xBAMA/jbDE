#version 430

uniform sampler2D sphere;
uniform mat3 trident;
uniform float frameHeight;

// moving lights state
uniform int lightCount;

struct light_t {
	vec4 position;
	vec4 color;
};

layout( binding = 4, std430 ) buffer lightDataBuffer {
	light_t lightData[];
};

in float radius;
in vec3 color;
in vec3 position;
in mat3 rot;
flat in int index; // flat means no interpolation across primitive

out vec4 glFragColor;
layout( depth_greater ) out float gl_FragDepth;

void main () {
	vec4 tRead = texture( sphere, gl_PointCoord.xy );
	if ( tRead.x < 0.05f ) discard;

	vec3 normal = inverse( trident ) * vec3( 2.0f * ( gl_PointCoord.xy - vec2( 0.5f ) ), -tRead.x );
	vec3 worldPosition = position + normal * ( radius / frameHeight );

	// lighting calculations
	vec3 lightColor = vec3( 0.0f );
	for ( int i = 0; i < lightCount; i++ ) {
		const float lightDist = distance( rot * lightData[ i ].position.xyz, position );
		lightColor += lightData[ i ].color.rgb * ( 1.0f / lightDist );
	}

	gl_FragDepth = gl_FragCoord.z + ( ( radius / frameHeight ) * ( 1.0f - tRead.x ) );
	glFragColor = vec4( color * lightContribution, 1.0f );
}
#version 430

uniform sampler2D sphere;
uniform float time;
uniform float scale;
uniform float AR;
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
in vec3 worldPosition;
in float height;
flat in int index; // flat means no interpolation across primitive

out vec4 glFragColor;
layout( depth_greater ) out float gl_FragDepth;

void main () {
	vec4 tRead = texture( sphere, gl_PointCoord.xy );
	if ( tRead.x < 0.05f ) discard;

	const mat3 inverseTrident = inverse( trident );

	vec3 normal = inverseTrident * vec3( 2.0f * ( gl_PointCoord.xy - vec2( 0.5f ) ), -tRead.x );
	vec3 worldPosition_adjusted = worldPosition - normal * ( radius / frameHeight );

	// lighting
	const vec3 eyePosition = vec3( 0.0f, 0.0f, -5.0f );
	const vec3 viewVector = inverseTrident * normalize( eyePosition - worldPosition_adjusted );
	vec3 lightContribution = vec3( 0.0f );

	float dMin = 1000.0f;

	for ( int i = 0; i < lightCount; i++ ) {
		// phong setup
		const vec3 lightLocation = lightData[ i ].position.xyz;
		const vec3 lightVector = normalize( lightLocation - worldPosition_adjusted );
		const vec3 reflectedVector = normalize( reflect( lightVector, normal ) );

		const float lightDot = dot( normal, lightVector );
		const float dLight = distance( worldPosition_adjusted, lightLocation );

		dMin = min( dLight, dMin );

		// todo: better attenuation function than inverse square - parameterize with extra data fields in the ssbo
			// https://lisyarus.github.io/blog/graphics/2022/07/30/point-light-attenuation.html

		// lighting calculation
		const float distanceFactor = min( 1.0f / ( pow( dLight, 2.0f ) ), 5.0f );
		const float diffuseContribution = distanceFactor * max( lightDot, 0.0f );
		const float specularContribution = distanceFactor * pow( max( dot( reflectedVector, viewVector ), 0.0f ), 60.0f );

		lightContribution += lightData[ i ].color.rgb * ( diffuseContribution + ( ( lightDot > 0.0f ) ? specularContribution : 0.0f ) );
	}

	gl_FragDepth = gl_FragCoord.z + ( ( radius / frameHeight ) * ( 1.0f - tRead.x ) );
	glFragColor = vec4( color * lightContribution, 1.0f );
	// glFragColor = vec4( normal, 1.0f );
	// glFragColor = vec4( 0.2f / worldPosition_adjusted, 1.0f );

	// glFragColor = vec4( ( dot( viewVector, normal ) < 0.9f ) ? vec3( 1.0f ) : vec3( 0.0f ), 1.0f ); // black dot on white sphere, at center if we are looking at it

	// if ( dMin < 0.3f ) {
	// 	glFragColor = vec4( color * lightContribution, 1.0f );
	// } else {
	// 	glFragColor = vec4( 0.0f, 0.0f, 0.0f, 1.0f );
	// }
}
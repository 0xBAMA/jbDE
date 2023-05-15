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
in vec3 position;
flat in int index; // flat means no interpolation across primitive

out vec4 glFragColor;
layout( depth_greater ) out float gl_FragDepth;

void main () {
	vec4 tRead = texture( sphere, gl_PointCoord.xy );
	if ( tRead.x < 0.05f ) discard;

	vec3 normal = inverse( trident ) * vec3( 2.0f * ( gl_PointCoord.xy - vec2( 0.5f ) ), -tRead.x );
	vec3 worldPosition = position + normal * ( radius / frameHeight );

	// lighting
	const vec3 eyePosition = vec3( 0.0f, 0.0f, -1.0f );
	vec3 lightContribution = vec3( 0.0f );
	for ( int i = 0; i < lightCount; i++ ) {
		const vec3 lightLocation = scale * trident * lightData[ i ].position.xyz;

		// phong setup
		const vec3 lightVector = normalize( worldPosition - lightLocation );
		const vec3 viewVector = normalize( worldPosition - eyePosition );
		const vec3 reflectedVector = normalize( reflect( lightVector, normal ) );

		// lighting calculation
		const float lightDot = dot( normal, lightVector );
		const float distanceFactor = 1.0f / ( pow( distance( worldPosition, lightLocation ) / scale, 2.0f ) );
		const float diffuseContribution = distanceFactor * max( lightDot, 0.0f );
		const float specularContribution = distanceFactor * pow( max( dot( reflectedVector, viewVector ), 0.0f ), 60.0f );

		lightContribution += lightData[ i ].color.rgb * ( diffuseContribution + ( ( lightDot > 0.0f ) ? specularContribution : 0.0f ) );
	}

	gl_FragDepth = gl_FragCoord.z + ( ( radius / frameHeight ) * ( 1.0f - tRead.x ) );
	glFragColor = vec4( color * lightContribution, 1.0f );
}
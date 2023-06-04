#version 430

uniform sampler2D sphere;
uniform mat3 trident;
uniform float time;
uniform float scale;
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
in float roughness;
in vec3 color;
in vec3 worldPosition;
flat in int index;

layout( depth_greater ) out float gl_FragDepth;

layout( location = 0 ) out vec4 glFragColor;
layout( location = 1 ) out vec4 normalResult;
layout( location = 2 ) out vec4 positionResult;

void main () {
	vec4 tRead = texture( sphere, gl_PointCoord.xy );
	if ( tRead.x < 0.05f ) discard;

	const mat3 inverseTrident = inverse( trident );

	vec3 normal = inverseTrident * vec3( 2.0f * ( gl_PointCoord.xy - vec2( 0.5f ) ), -tRead.x );
	vec3 worldPosition_adjusted = worldPosition + normal * ( radius / frameHeight );

	// // lighting
	// const float maxDistFactor = 5.0f;
	// const vec3 eyePosition = vec3( 0.0f, 0.0f, -5.0f );
	// const vec3 viewVector = inverseTrident * normalize( eyePosition - worldPosition_adjusted );
	// vec3 lightContribution = vec3( 0.0f );

	// for ( int i = 0; i < lightCount; i++ ) {
	// 	// phong setup
	// 	const vec3 lightLocation = lightData[ i ].position.xyz;
	// 	const vec3 lightVector = normalize( lightLocation - worldPosition_adjusted );
	// 	const vec3 reflectedVector = normalize( reflect( lightVector, normal ) );

	// 	// lighting calculation
	// 	const float lightDot = dot( normal, lightVector );
	// 	const float dLight = distance( worldPosition_adjusted, lightLocation );

	// 	const float distanceFactor = min( 1.0f / ( pow( dLight, 2.0f ) ), maxDistFactor );
	// 	const float diffuseContribution = distanceFactor * max( lightDot, 0.0f );
	// 	const float specularContribution = distanceFactor * pow( max( dot( -reflectedVector, viewVector ), 0.0f ), roughness );

	// 	lightContribution += lightData[ i ].color.rgb * ( diffuseContribution + ( ( lightDot > 0.0f ) ? specularContribution : 0.0f ) );
	// }

	// glFragColor = vec4( color * lightContribution, 1.0f );

	gl_FragDepth = gl_FragCoord.z + ( ( radius / frameHeight ) * ( 1.0f - tRead.x ) );
	glFragColor = vec4( color, 1.0f );
	normalResult = vec4( normal, 1.0f );
	positionResult = vec4( worldPosition_adjusted, 1.0f );
}
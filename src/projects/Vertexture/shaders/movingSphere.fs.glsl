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

	gl_FragDepth = gl_FragCoord.z + ( ( radius / frameHeight ) * ( 1.0f - tRead.x ) );
	glFragColor = vec4( color, 1.0f );
	normalResult = vec4( normal, 1.0f );
	positionResult = vec4( worldPosition_adjusted, 1.0f );
}
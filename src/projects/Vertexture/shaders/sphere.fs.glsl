#version 430

uniform sampler2D sphere;
uniform mat3 trident;
uniform float time;
uniform vec3 lightDirection;

// moving lights state
uniform int lightCount;

struct light_t {
	vec4 position;
	vec4 color;
};

layout( binding = 4, std430 ) buffer lightData {
	light_t data[];
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

	vec3 normal = inverse( rot ) * vec3( 2.0f * ( gl_PointCoord.xy - vec2( 0.5f ) ), -tRead.x );
	vec3 worldPosition = position + normal * ( radius / 1024.0f );

	gl_FragDepth = gl_FragCoord.z + ( ( radius / 1024.0f ) * ( 1.0f - tRead.x ) );
	glFragColor = vec4( tRead.xyz * color * clamp( dot( rot * normal, lightDirection ), 0.1618f, 1.0f ) * 1.3f, 1.0f );
}
#version 430

out vec4 glFragColor;

in float radius;
in vec3 color;
in vec3 position;
flat in int index;

in mat3 rot;

uniform sampler2D sphere;
uniform mat3 trident;
uniform float time;

uniform vec3 lightDirection;

layout( depth_greater ) out float gl_FragDepth;

void main() {

	vec4 tRead = texture( sphere, gl_PointCoord.xy );
	if ( tRead.x < 0.05f ) discard;

	// vec3 normal = -( transpose( inverse( trident ) ) * vec3( 2.0f * ( gl_PointCoord.xy - vec2( 0.5f ) ), tRead.x ) );
	vec3 normal = inverse( rot ) * vec3( 2.0f * ( gl_PointCoord.xy - vec2( 0.5f ) ), -tRead.x );
	vec3 worldPosition = position;

	// gl_FragDepth = gl_FragCoord.z - ( ( radius / 1024.0f ) * tRead.x );
	gl_FragDepth = gl_FragCoord.z + ( ( radius / 1024.0f ) * ( 1.0f - tRead.x ) );

	glFragColor = vec4( tRead.xyz * color * clamp( dot( rot * normal, lightDirection ), 0.1618f, 1.0f ) * 1.3f, 1.0f );
}
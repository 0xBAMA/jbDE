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

void main() {

	vec4 tRead = texture( sphere, gl_PointCoord.xy );
	if ( tRead.x < 0.05f ) discard;

	// vec3 normal = -( transpose( inverse( trident ) ) * vec3( 2.0f * ( gl_PointCoord.xy - vec2( 0.5f ) ), tRead.x ) );
	vec3 normal = inverse( rot ) * vec3( 2.0f * ( gl_PointCoord.xy - vec2( 0.5f ) ), -tRead.x );
	vec3 worldPosition = position;

	gl_FragDepth = gl_FragCoord.z - ( ( radius / 1400.0f ) * tRead.x );
	// glFragColor = vec4( vec3( dot( trident * vec3( -2.0, 1.0, 1.0 + 2.5 * sin( time * 10.0f ) ), normal ) ), 1.0f );
	glFragColor = vec4( normal, 1.0f );
	// glFragColor = vec4( trident * normal, 1.0f );
	// glFragColor = vec4( worldPosition, 1.0f );
	// glFragColor = vec4( tRead.xyz * color, 1.0f );
	// glFragColor = vec4( tRead.xyz * color * dot( normal, vec3( 1.0f ) ), 1.0f );
	// glFragColor = vec4( tRead.xyz * color * clamp( dot( normal, vec3( 1.0f ) ), 0.1618f, 1.0f ) * 2.5f, 1.0f );
	// glFragColor = vec4( vec3( distance( worldPosition, vec3( 1.0f ) ) ) * 0.2f, 1.0f );
	// glFragColor = vec4( tRead.xyz * color * ( 1.4f - gl_FragDepth ), 1.0f );
	// glFragColor = vec4( tRead.xyz * color * ( 1.4f - gl_FragDepth * 2.0f ), 1.0f );
	// glFragColor = vec4( tRead.xyz * color * ( float( index ) / 20000 ) , 1.0f );
}
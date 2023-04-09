#version 430

out vec4 glFragColor;

in float radius;
in vec3 color;
in vec3 position;
flat in int index;

uniform sampler2D sphere;
uniform mat3 trident;
uniform float time;

void main() {

	vec4 tRead = texture( sphere, gl_PointCoord.xy );
	if ( tRead.x < 0.05f ) discard;

	// vec3 normal = trident * vec3( 2.0f * ( gl_PointCoord.xy - vec2( 0.5f ) ), tRead.x );
	vec3 normal = transpose( inverse( trident ) ) * vec3( 2.0f * ( gl_PointCoord.xy - vec2( 0.5f ) ), tRead.x );
	vec3 worldPosition = position + normal * ( radius / 1400.0f );

	gl_FragDepth = gl_FragCoord.z - ( ( radius / 1400.0f ) * tRead.x );
	// glFragColor = vec4( vec3( dot( trident * vec3( -2.0, 1.0, 1.0 + 2.5 * sin( time * 10.0f ) ), normal ) ), 1.0f );
	// glFragColor = vec4( normal, 1.0f );
	// glFragColor = vec4( trident * normal, 1.0f );
	// glFragColor = vec4( tRead.xyz * color, 1.0f );
	glFragColor = vec4( tRead.xyz * color * ( 1.4f - gl_FragDepth ), 1.0f );
	// glFragColor = vec4( tRead.xyz * color * ( float( index ) / 20000 ) , 1.0f );
}
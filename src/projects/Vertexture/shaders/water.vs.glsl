#version 430

uniform float AR;

in vec3 vPosition;
// in vec2 vTexturePosition;

out vec3 color;
out vec3 normal;

uniform sampler2D colorMap;
uniform sampler2D normalMap;
uniform sampler2D heightMap;

uniform float time;
uniform mat3 trident;

void main() {

	// const vec2 texCoord = ( vPosition.xy / ( 1.618f * 2.0f ) ) * 7.0f + vec2( 0.08f * time );
	const vec2 texCoord = ( vPosition.xy / ( 1.618f * 2.0f ) ) + vec2( 0.08f * time );
	vec4 cRead = texture( colorMap, texCoord );
	vec4 nRead = texture( normalMap, texCoord );
	vec4 hRead = texture( heightMap, texCoord );

	color = cRead.xyz / 2.0f;
	normal = nRead.xyz;

	hRead.r *= 0.1f * ( sin( 0.08f * time ) + 1.0f ) * sin( vPosition.x * vPosition.y * 0.01f );

	const vec3 vPosition_local = trident * ( vPosition + vec3( 0.0f, 0.0f, hRead.r * 0.01f ) );
	// const vec3 vPosition_local = trident * vPosition;
	gl_Position = vec4( vPosition_local * vec3( 1.0f, AR, 1.0f ) * 0.4f, 1.0f );
}

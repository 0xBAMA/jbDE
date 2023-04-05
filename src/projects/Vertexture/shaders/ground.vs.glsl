#version 430

uniform float AR;

in vec3 vPosition;
// in vec2 vTexturePosition;

out vec3 color;
uniform sampler2D heightmap;
uniform float time;

mat4 RotationMatrix ( vec3 axis, float angle ) {
	axis = normalize( axis );
	float s = sin( angle );
	float c = cos( angle );
	float oc = 1.0f - c;
	return mat4(oc * axis.x * axis.x + c,			oc * axis.x * axis.y - axis.z * s,	oc * axis.z * axis.x + axis.y * s,	0.0f,
				oc * axis.x * axis.y + axis.z * s,	oc * axis.y * axis.y + c,			oc * axis.y * axis.z - axis.x * s,	0.0f,
				oc * axis.z * axis.x - axis.y * s,	oc * axis.y * axis.z + axis.x * s,	oc * axis.z * axis.z + c,			0.0f,
				0.0f,								0.0f,								0.0f,								1.0f );
}

void main() {

	vec4 tRead = texture( heightmap, vPosition.xy / ( 1.618f * 2.0f ) );

	color = vec3( tRead.r );
	color.r *= 0.7f;
	color.g *= 0.5f;
	color.b *= 0.4f;

	vec3 vPosition_local = ( // I don't like this, move it to the CPU
		RotationMatrix( vec3( 0.0f, 1.0f, 0.0f ), 0.25f + cos( time ) * 0.1f ) *
		RotationMatrix( vec3( 1.0f, 0.0f, 0.0f ), 2.15f ) *
		RotationMatrix( vec3( 0.0f, 0.0f, 1.0f ), 0.5f + sin( time ) * 0.3f ) *
		vec4( vPosition + vec3( 0.0f, 0.0f, tRead.r * 0.3f - 0.15f ), 1.0f ) ).xyz;

	gl_Position = vec4( vPosition_local * vec3( 1.0f, AR, 1.0f ) * 0.4f, 1.0f );
}

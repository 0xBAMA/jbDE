#version 430

in vec4 vofiColor;
out vec4 outColor;

vec2 RaySphereIntersect ( vec3 r0, vec3 rd, vec3 s0, float sr ) {
	// r0 is ray origin
	// rd is ray direction
	// s0 is sphere center
	// sr is sphere radius
	// return is the roots of the quadratic, includes negative hits
	float a = dot( rd, rd );
	vec3 s0_r0 = r0 - s0;
	float b = 2.0f * dot( rd, s0_r0 );
	float c = dot( s0_r0, s0_r0 ) - ( sr * sr );
	float disc = b * b - 4.0f * a * c;
	if ( disc < 0.0f ) {
		return vec2( -1.0f, -1.0f );
	} else {
		return vec2( -b - sqrt( disc ), -b + sqrt( disc ) ) / ( 2.0f * a );
	}
}

void main() {
	const vec3 eyePos = vec3( 2.0f, 0.0f, 0.0f );
	vec2 result = RaySphereIntersect( eyePos, vofiColor.rgb - eyePos, vec3( 0.0f ), 0.5f );
	if ( result == vec2( -1.0f, -1.0f ) && ( int( gl_FragCoord.x ) % 2 == 0 ) ) {
		discard;
	} else {
		// outColor = vofiColor;

		// fractional depth... need to figure out a more consistent way to handle this
		gl_FragDepth = result.x / 10.0f;
		outColor = vec4( normalize( vofiColor.rgb ), 1.0f );
	}
}
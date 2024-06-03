#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// texture for the atomic writes
layout( r32ui ) uniform uimage2D vectorscopeImage;

// texture for the composited output
layout( rgba8 ) uniform image2D compositedResult;

// buffer for max pixel count
layout( binding = 5, std430 ) buffer perColumnMins { uint maxCount; };

vec3 rgb2hsv( vec3 c ) {
	vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
	vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

	float d = q.x - min(q.w, q.y);
	float e = 1.0e-10;
	return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 hsv2rgb( in vec3 c ) {
	vec3 rgb = clamp( abs(mod(c.x*6.0+vec3(0.0,4.0,2.0),6.0)-3.0)-1.0, 0.0, 1.0 );
	// rgb = rgb*rgb*(3.0-2.0*rgb); // cubic smoothing
	return c.z * mix( vec3(1.0), rgb, c.y);
}


void main () {
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	const vec2 normalizedSpace = ( vec2( loc ) + vec2( 0.5f ) ) / imageSize( compositedResult ) - vec2( 0.5f );

	vec4 color = vec4( 0.0f, 0.0f, 0.0f, 1.0f );
	const float radius = sqrt( dot( normalizedSpace, normalizedSpace ) );
	if ( radius < 0.5f ) {
		// todo: gridlines, background
		// smoothstep for edge falloff

		// find the pixel count value, normalize by dividing by maxCount
		const float value = 2.0f * float( imageLoad( vectorscopeImage, loc ).r ) / float( maxCount );

		// find the color for this pixel, from position -> hsv color, set l=value -> rgb
		color.rgb = hsv2rgb( vec3( atan( normalizedSpace.x, normalizedSpace.y ) / 6.28f, radius * 2.0f, value ) );
	} else {
		color.a = smoothstep( 0.5f, 0.4f, radius );
	}

	imageStore( compositedResult, loc, color );
}
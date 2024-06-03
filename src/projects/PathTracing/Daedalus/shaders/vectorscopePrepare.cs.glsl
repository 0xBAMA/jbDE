#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// the source data, that we are evaluating
layout( rgba32f ) uniform image2D tonemappedSourceData;

// texture for the atomic writes
layout( r32ui ) uniform uimage2D vectorscopeImage;

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
	if ( all( lessThan( loc, imageSize( tonemappedSourceData ) ) ) ) {
		vec4 pixelColor = imageLoad( tonemappedSourceData, loc );

		// find hue and saturation, by converting rgb to hsv
		const vec3 hsvColor = rgb2hsv( pixelColor.rgb );
		const float hue = hsvColor.r * 6.28f;
		const float sat = hsvColor.g;
		const ivec2 writePoint = ivec2( ( vec2(
			sat * cos( hue ),
			sat * sin( hue )
		) * 0.5f + vec2( 0.5f ) ) * imageSize( vectorscopeImage ) );

		// increment the plot + update maxcount
		atomicMax( maxCount, imageAtomicAdd( vectorscopeImage, writePoint, 1 ) + 1 );
	}
}
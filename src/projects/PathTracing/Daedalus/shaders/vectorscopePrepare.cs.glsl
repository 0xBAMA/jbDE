#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// the source data, that we are evaluating
layout( rgba32f ) uniform image2D tonemappedSourceData;

// texture for the atomic writes
layout( r32ui ) uniform uimage2D vectorscopeImage;

// buffer for max pixel count
layout( binding = 5, std430 ) buffer perColumnMins { uint maxCount; };

// rgb <-> oklch
#include "oklabConversions.h.glsl"

void main () {
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );
	if ( all( lessThan( loc, imageSize( tonemappedSourceData ) ) ) ) {
		vec4 pixelColor = imageLoad( tonemappedSourceData, loc );

		// find hue and chroma, by converting rgb to lch
		const vec3 lchColor = srgb_to_oklch( pixelColor.rgb );
		const float chroma = lchColor.g;
		const float hue = lchColor.b;
		const ivec2 writePoint = ivec2( ( vec2(
			chroma * cos( hue ),
			chroma * sin( hue )
		) * 0.5f + vec2( 0.5f ) ) * imageSize( vectorscopeImage ) );

		// increment the plot + update maxcount
		atomicMax( maxCount, imageAtomicAdd( vectorscopeImage, writePoint, 1 ) + 1 );
	}
}
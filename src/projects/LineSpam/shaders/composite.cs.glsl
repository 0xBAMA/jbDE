#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// tally buffers
layout( binding = 0, r32ui ) uniform uimage2D redTally;
layout( binding = 1, r32ui ) uniform uimage2D greenTally;
layout( binding = 2, r32ui ) uniform uimage2D blueTally;
layout( binding = 3, r32ui ) uniform uimage2D sampleTally;

// depth buffer, for clearing
layout( binding = 4, r32ui ) uniform uimage2D depthBuffer;

// id buffer
layout( binding = 5, r32ui ) uniform uimage2D idBuffer;

// color attachment
layout( binding = 6, rgba16f ) uniform image2D compositedResult;

// opaque line buffer
struct line_t {
	vec4 p0;
	vec4 p1;
	vec4 color;
};

layout( binding = 0, std430 ) buffer opaqueLineData {
	line_t opaqueData[];
};

vec4 blendColor( in vec4 under, in vec4 over ) {
	vec4 color = under;
	color.a = max( over.a + color.a * ( 1.0f - over.a ), 0.001f );
	color.rgb = over.rgb * over.a + color.rgb * color.a * ( 1.0f - color.a );
	color.rgb /= color.a; // missing piece of a over b alpha blending - can't really see a lot of difference
	return color;
}

void main() {
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// opaque color comes from index loaded from id buffer, indexing into the SSBO for opaqueData[ id ].color
	const int idx = imageLoad( idBuffer, loc ).r - 1;
	vec4 opaqueColor = ( idx == 0 ) ? vec4( 0.0f ) : vec4( opaqueData[ idx ].color, 1.0f );

	// transparent color resolve
	vec4 tallySamples = vec4(
		float( imageLoad( redTally, loc ).r ),
		float( imageLoad( greenTally, loc ).r ),
		float( imageLoad( blueTally, loc ).r ),
		float( imageLoad( sampleTally, loc ).r )
	);
	// averaging out colors and get a Beer's-law-ish term for density
	vec4 transparentColor = vec4( tallySamples.rgb / tallySamples.a, exp( -0.003f * tallySamples.a ) );

	// blend the transparent over the opaque
	vec3 finalColor = blendColor( opaqueColor, transparentColor );

	// store back in the composited result
	imageStore( compositedResult, loc, vec4( finalColor, 1.0f ) );

	// also do the clears here
	imageStore( redTally, loc, uvec4( 0 ) );
	imageStore( greenTally, loc, uvec4( 0 ) );
	imageStore( blueTally, loc, uvec4( 0 ) );
	imageStore( sampleTally, loc, uvec4( 0 ) );
	imageStore( depthBuffer, loc, uvec4( 0 ) );
	imageStore( idBuffer, loc, uvec4( 0 ) );
}
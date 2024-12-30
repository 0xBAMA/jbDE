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

// line buffers
struct line_t {
	vec4 p0;
	vec4 p1;
	vec4 color0;
	vec4 color1;
};
layout( binding = 0, std430 ) buffer opaqueLineData { line_t opaqueData[]; };
layout( binding = 1, std430 ) buffer transparentLineData { line_t transparentData[]; };

vec4 blendColor( in vec4 under, in vec4 over ) {
	float a0 = ( over.a + under.a * ( 1.0f - over.a ) );
	return vec4( ( over.rgb * over.a + under.rgb * under.a * ( 1.0f - over.a ) ) / a0, a0 );
}

void main() {
	const ivec2 loc = ivec2( gl_GlobalInvocationID.xy );

	// opaque color comes from index loaded from id buffer, indexing into the SSBO for opaqueData[ id ].color
	const uint idx = uint( imageLoad( idBuffer, loc ).r ) - 1;
	vec4 opaqueColor = ( idx == 0 ) ? vec4( 0.0f ) : opaqueData[ idx ].color;
	// opaqueColor.rgb *= 1000.0f;

	// transparent color resolve
	vec4 tallySamples = vec4( // similar scheme to Icarus
		float( imageLoad( redTally, loc ).r ) / ( 256.0f * 1024.0f ),
		float( imageLoad( greenTally, loc ).r ) / ( 256.0f * 1024.0f ),
		float( imageLoad( blueTally, loc ).r ) / ( 256.0f * 1024.0f ),
		float( imageLoad( sampleTally, loc ).r ) / 256.0f
	);
	// averaging out colors and get a Beer's-law-ish term for density
	// vec4 transparentColor = vec4( tallySamples.rgb / tallySamples.a, exp( -0.003f * tallySamples.a ) );
	vec4 transparentColor = vec4( tallySamples.rgb, clamp( exp( -0.03f * tallySamples.a ), 0.0f, 1.0f ) );

	// blend the transparent over the opaque
	vec3 finalColor = ( transparentColor.a == 0.0f ) ? opaqueColor.rgb : blendColor( opaqueColor, transparentColor ).rgb;

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
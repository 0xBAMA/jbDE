#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

// tally buffers
layout( binding = 0, r32ui ) uniform uimage2D redTally;
layout( binding = 1, r32ui ) uniform uimage2D greenTally;
layout( binding = 2, r32ui ) uniform uimage2D blueTally;
layout( binding = 3, r32ui ) uniform uimage2D sampleTally;

// depth buffer probably not needed
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

void main() {
	// opaque color comes from index loaded from id buffer, indexing into the SSBO for opaqueData[ id ].color

	// transparent color

	// blend the transparent over the opaque
	vec3 finalColor = vec3( 1.0f, 0.0f, 0.0f );

	// store back in the composited result
	imageStore( compositedResult, ivec2( gl_GlobalInvocationID.xy ), vec4( finalColor, 1.0f ) );
}
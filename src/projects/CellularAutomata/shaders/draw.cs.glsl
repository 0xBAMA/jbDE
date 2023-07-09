#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

// current state of the Cellular automata
layout( binding = 2 ) uniform usampler2D CAStateBuffer;
uniform vec2 resolution;

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );

	// current state of the cellular automata
	vec3 result = texture( CAStateBuffer, ( vec2( writeLoc ) + vec2( 0.5f ) ) / resolution ).xxx * 255.0f;
	// vec3 result = texture( CAStateBuffer, vec2( writeLoc ) / resolution ).xxx * 255.0f;

	// write the data to the image
	imageStore( accumulatorTexture, writeLoc, vec4( result / 255.0f, 1.0f ) );
}

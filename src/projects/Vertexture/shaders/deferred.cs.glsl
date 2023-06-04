#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;
layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;

uniform sampler2D depthTexture;
uniform sampler2D colorTexture;
uniform sampler2D normalTexture;
uniform sampler2D positionTexture;

uniform vec2 resolution;

void main () {
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	// uvec3 result = uvec3( imageLoad( blueNoiseTexture, writeLoc % imageSize( blueNoiseTexture ) ).r * 0.1618f );
	// imageStore( accumulatorTexture, writeLoc, uvec4( result, 255 ) );

	vec2 sampleLocation = ( vec2( writeLoc ) + vec2( 0.5f ) ) / resolution;
	sampleLocation.y = 1.0f - sampleLocation.y;
	vec4 color = texture( colorTexture, sampleLocation );
	vec4 depth = texture( depthTexture, sampleLocation );
	vec4 normal = texture( normalTexture, sampleLocation );
	vec4 position = texture( positionTexture, sampleLocation );

	// eventually, move the lighting calcs here

	// vec3 outputValue = ( 1.0f - depth.r ) * color.rgb;
	// vec3 outputValue *= normal.xyz;
	// vec3 outputValue = abs( position.rgb );
	vec3 outputValue = 0.5f * ( normal.rgb + vec3( 1.0f ) );

	imageStore( accumulatorTexture, writeLoc, uvec4( uvec3( outputValue * 255 ), 255 ) );
}

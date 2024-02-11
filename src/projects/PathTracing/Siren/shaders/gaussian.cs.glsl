#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( rgba32f ) uniform image2D sourceData;
layout( rgba32f ) uniform image2D destData;

void main () {
	// write the data to the image
	const ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	const float sampleCount = imageLoad( sourceData, writeLoc ).a;
	vec3 kernelResult = ( // 3x3 gaussian kernel
		1.0f * imageLoad( sourceData, writeLoc + ivec2( -1, -1 ) ).rgb +
		1.0f * imageLoad( sourceData, writeLoc + ivec2( -1,  1 ) ).rgb +
		1.0f * imageLoad( sourceData, writeLoc + ivec2(  1, -1 ) ).rgb +
		1.0f * imageLoad( sourceData, writeLoc + ivec2(  1,  1 ) ).rgb +
		2.0f * imageLoad( sourceData, writeLoc + ivec2(  0,  1 ) ).rgb +
		2.0f * imageLoad( sourceData, writeLoc + ivec2(  0, -1 ) ).rgb +
		2.0f * imageLoad( sourceData, writeLoc + ivec2(  1,  0 ) ).rgb +
		2.0f * imageLoad( sourceData, writeLoc + ivec2( -1,  0 ) ).rgb +
		4.0f * imageLoad( sourceData, writeLoc + ivec2(  0,  0 ) ).rgb ) / 16.0f;

	imageStore( destData, writeLoc, vec4( kernelResult, sampleCount ) );
}

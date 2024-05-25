#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( rgba32f ) uniform image2D sourceData;
layout( rgba32f ) uniform image2D destData;

uniform float blendAmount;

void main () {
	// write the data to the image
	ivec2 writeLoc = ivec2( gl_GlobalInvocationID.xy );
	vec4 color = imageLoad( sourceData, writeLoc );
	vec4 prevColor = imageLoad( destData, writeLoc );
	imageStore( destData, writeLoc, mix( prevColor, color, blendAmount ) );
}

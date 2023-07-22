#version 430
layout( local_size_x = 16, local_size_y = 16, local_size_z = 1 ) in;

layout( r32f ) uniform image2D sourceData;
layout( rgba8ui ) uniform uimage2D map;

void main () {
	ivec2 location = ivec2( gl_GlobalInvocationID.xy );
	float src = 255.0f * imageLoad( sourceData, location ).r;
	imageStore( map, location, uvec4( src, src, src, src ) );
}

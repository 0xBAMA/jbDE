#version 430
layout( local_size_x = 16, local_size_y = 1, local_size_z = 1 ) in;

// layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( binding = 1, rgba16f ) uniform image2D accumulatorTexture;
layout( binding = 2, rgba8ui ) uniform uimage2D map;

// uniform float time;


void main () {
	
}
#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

// layout( binding = 0, rgba8ui ) uniform uimage2D blueNoiseTexture;

layout( rgba8ui ) uniform uimage2D map; // convert to sampler? tbd
layout( rgba16f ) uniform image2D target;

// parameters
uniform ivec2 resolution;	// current screen resolution
uniform vec2 viewPosition;	// starting point for rendering
uniform int viewerHeight;	// viewer height
uniform float viewAngle;	// view direction angle
uniform float maxDistance;	// maximum traversal
uniform int horizonLine;	// horizon line position
uniform float heightScalar;	// scale value for height
uniform float offsetScalar;	// scale the side-to-side spread
uniform float fogScalar;	// scalar for fog distance
uniform float stepIncrement;// increase in step size as you get farther from the camera
uniform float FoVScalar;	// adjustment for the FoV

void main () {

	// imageStore( target, ivec2( gl_GlobalInvocationID.xy ), vec4( 1.0f, 0.0f, 0.0f, 0.2f ) );

}
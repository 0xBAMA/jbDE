#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

layout( rgba8ui ) uniform uimage2D blueNoiseTexture;
layout( rgba8ui ) uniform uimage2D map; // convert to sampler? tbd
layout( rgba16f ) uniform image2D target;

uniform ivec2 resolution;			// current screen resolution
uniform vec2 viewPosition;			// starting point for rendering
uniform float viewAngle;			// view direction angle
const float maxDistance = 500.0f;	// maximum traversal
const int viewerHeight = 300;		// viewer height
const int horizonLine = 1000;		// horizon line position
const float heightScalar = 170.0f;	// scale value for height
const float offsetScalar = 110.0f;	// scale the side-to-side spread
const float FoVScalar = 0.275f;		// adjustment for the FoV

vec2 globalForwards = vec2( 0.0f );

void DrawVerticalLine ( const uint x, const int yBottom, const int yTop, const vec4 col ) {
	const int yMin = clamp( yBottom, 0, imageSize( target ).y );
	const int yMax = clamp( yTop, 0, imageSize( target ).y );
	if ( yMin > yMax ) return;
	for ( int y = yMin; y < yMax; y++ ) {
		imageStore( target, ivec2( x, y ), col );
	}
}

mat2 Rotate2D ( float r ) {
	return mat2( cos( r ), sin( r ), -sin( r ), cos( r ) );
}

void main () {
	const float wPixels		= resolution.x;
	const float hPixels		= imageSize( target ).y;
	const uint myXIndex		= uint( gl_GlobalInvocationID.x );
	float yBuffer			= 15.0f * hPixels / 24.0f;

	// no reason to run outside the width of the minimap
	if ( myXIndex > wPixels ) return;

	// initial clear
	DrawVerticalLine( myXIndex, int( yBuffer ), int( hPixels ), vec4( 0.0f, 0.0f, 0.0f, 0.0f ) );

	// FoV considerations
		// mapping [0..wPixels] to [-1..1]
	float FoVAdjust			= -1.0f + float( myXIndex ) * ( 2.0f ) / float( wPixels );

	const mat2 rotation		= Rotate2D( viewAngle + FoVAdjust * FoVScalar );
	const vec2 direction	= rotation * vec2( 1.0f, 0.0f );
	vec2 startCenter		= viewPosition - 200.0f * direction;
	const vec2 forwards		= globalForwards = Rotate2D( viewAngle ) * vec2( 1.0f, 0.0f );
	const vec2 fixedDirection = direction * ( dot( direction, forwards ) / dot( forwards, forwards ) );
	vec3 sideVector			= cross( vec3( forwards.x, 0.0f, forwards.y ), vec3( 0.0f, 1.0f, 0.0f ) );
	vec2 viewPositionLocal	= startCenter + sideVector.xz * FoVAdjust * offsetScalar;

	// draw the vertical strip of pixels for myXIndex
	const float dz = 0.25f;
	for ( float dSample = 1.0f; dSample < maxDistance && yBuffer < hPixels; dSample += dz ) {
		const ivec2 samplePosition	= ivec2( viewPositionLocal + dSample * direction );
		const uvec4 dataSample		= imageLoad( map, samplePosition );
		const float heightOnScreen	= ( ( float( dataSample.a ) - viewerHeight ) * ( 1.0f / dSample ) * heightScalar + horizonLine );
		if ( heightOnScreen > yBuffer ) {
			DrawVerticalLine( myXIndex, int( yBuffer ), int( heightOnScreen ), vec4( dataSample.xyz, 255.0f ) / 255.0f );
			yBuffer = uint( heightOnScreen );
		}
	}
}
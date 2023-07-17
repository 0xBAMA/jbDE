// #version 430 core

// // shader invocation details
// layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

// // texture working set
// layout( binding = 0, rgba8ui ) uniform uimage2D renderTexture;
// layout( binding = 1, rgba8ui ) uniform uimage2D minimapTexture;

// layout( binding = 2, rgba8ui ) uniform uimage2D map;

// uniform ivec2 resolution;// current screen resolution
// uniform vec2 viewPosition;// starting point for rendering
// uniform float viewBump;// how much to move the player back
// uniform float viewAngle;// view direction angle
// uniform float minimapScalar;// minimap scale factor

// // constants
// const int viewerHeight = 300;
// uniform int viewerElevation = 300;// how high the viewer is off the heightmap
// const float maxDistance = 400; // maximum traversal
// const int horizonLine = 1000;// horizon line position
// const float heightScalar = 150;// scale value for height
// const float offsetScalar = 110;// scale the side-to-side spread
// const float FoVScalar = 0.275;// adjustment for the FoV

// // need masked access to the heightmap - circular mask for the heightmap
// // center point being located some distance "in front of" the viewers position - along the direction vector

// uvec4 backgroundColor = uvec4( 255, 0, 0, 255 );

// // void drawVerticalLine( const uint x, const float yBottom, const float yTop, const uvec4 col ){
// void drawVerticalLine ( const uint x, const float yBottom, const float yTop, const vec4 col ) {
// 	const int yMin = clamp( int( yBottom ), 0, imageSize( renderTexture ).y );
// 	const int yMax = clamp( int(  yTop  ), 0, imageSize( renderTexture ).y );
// 	if ( yMin > yMax ) return;
// 	for ( int y = yMin; y < yMax; y++ ) {
// 		imageStore( minimapTexture, ivec2( x, y ), uvec4( col ) );
// 	}
// }

// vec2 globalForwards = vec2( 0.0f );

// bool insideMask ( vec2 queryLocation ) {
// 	return distance( queryLocation, viewPosition + vec2( viewBump * globalForwards ) ) < ( 100.0f / minimapScalar );
// }

// // uint heightmapReference(ivec2 location){
// float heightmapReference ( vec2 location ) {
// 	location = vec2( ( vec2( location - viewPosition ) * ( 1.0f / minimapScalar ) ) + viewPosition );
// 	location += vec2( viewBump * globalForwards );
// 	if ( insideMask( location ) ) {
// 		if ( distance( location, viewPosition ) < ( 1.618f / minimapScalar ) ) {
// 			return imageLoad( map, ivec2( location ) ).a + viewerElevation;
// 			// return ( texture( heightmap, location / textureSize( heightmap, 0 ) ).r * 255 ) + viewerElevation;
// 		}else{
// 			return imageLoad( map, ivec2( location ) ).a;
// 			// return ( texture( heightmap, location / textureSize( heightmap, 0 ) ).r * 255 );
// 		}
// 	} else {
// 		return 0.0f;
// 	}
// }

// // uvec4 colormapReference(ivec2 location){
// vec4 colormapReference ( vec2 location ) {
// 	location = vec2( ( vec2( location - viewPosition ) * ( 1.0f / minimapScalar ) ) + viewPosition );
// 	location += vec2( viewBump * globalForwards );
// 	if ( insideMask( location ) ) {
// 		if ( distance( location, viewPosition ) < ( 1.618f / minimapScalar ) ) {
// 			return vec4( 255.0f, 0.0f, 0.0f, 255.0f );
// 		} else if ( distance( location, viewPosition + vec2( viewBump * globalForwards ) ) > ( 97. / minimapScalar ) ) {
// 			return vec4( 0.0f );
// 		} else {
// 			// return texture( colormap, location / textureSize( colormap, 0 ) ) * 255.0f;
// 			return vec4( imageLoad( map, ivec2( location ) ).rgb, 255.0f );
// 		}
// 	} else {
// 		return vec4( 0.0f );
// 	}
// }

// mat2 rotate2D ( float r ) {
// 	return mat2( cos( r ), sin( r ), -sin( r ), cos( r ) );
// }

// void main () {
// 	const float wPixels		= resolution.x;
// 	const float hPixels		= imageSize( minimapTexture ).y;
// 	const float myXIndex	= gl_GlobalInvocationID.x;
// 	float yBuffer			= 13.0f * hPixels / 24.0f;

// 	if ( myXIndex > wPixels ) return;

// 	// FoV considerations
// 	//   mapping [0..wPixels] to [-1..1]
// 	float FoVAdjust			= -1.0f + float( myXIndex ) * ( 2.0f ) / float( wPixels );

// 	const mat2 rotation		= rotate2D( viewAngle + FoVAdjust * FoVScalar );
// 	const vec2 direction	= rotation * vec2( 1.0f, 0.0f );
// 	vec2 startCenter		= viewPosition - 200.0f * direction;

// 	const vec2 forwards		= globalForwards = rotate2D( viewAngle ) * vec2( 1.0f, 0.0f );
// 	const vec2 fixedDirection = direction * ( dot( direction, forwards ) / dot( forwards, forwards ) );

// 	// need a side-to-side adjust to give some spread - CPU side adjustment scalar needs to be added
// 	vec3 sideVector			= cross( vec3( forwards.x, 0.0f, forwards.y ), vec3( 0.0f, 1.0f, 0.0f ) );
// 	vec2 viewPositionLocal	= startCenter + sideVector.xz * FoVAdjust * offsetScalar;

// 	const float dz = 0.25f;
// 	for ( float dSample = 1.0f; dSample < maxDistance && yBuffer < hPixels; dSample += dz ) {
// 		const ivec2 samplePosition	= ivec2( viewPositionLocal + dSample * fixedDirection );
// 		const float heightSample	= heightmapReference( samplePosition );
// 		const float heightOnScreen	= ( ( heightSample - viewerHeight ) * ( 1.0f / dSample ) * heightScalar + horizonLine );
// 		if ( heightOnScreen > yBuffer ) {
// 			drawVerticalLine( int( myXIndex ), yBuffer, heightOnScreen, colormapReference( samplePosition ) );
// 			yBuffer = uint( heightOnScreen );
// 		}
// 	}
// }


#version 430
layout( local_size_x = 64, local_size_y = 1, local_size_z = 1 ) in;

layout( rgba8ui ) uniform uimage2D blueNoiseTexture;
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

// uvec4 blueNoiseRef ( ivec2 location ) {
// 	location.x = location.x % imageSize( blueNoiseTexture ).x;
// 	location.y = location.y % imageSize( blueNoiseTexture ).y;
// 	return imageLoad( blueNoiseTexture, location );
// }

void DrawVerticalLine ( const uint x, const int yBottom, const int yTop, const vec4 col ) {
	// const float blueNoiseScale = 255.0f * 127.0f;
	const int yMin = clamp( yBottom, 0, imageSize( target ).y );
	const int yMax = clamp( yTop, 0, imageSize( target ).y );
	if ( yMin > yMax ) return;
	for ( int y = yMin; y < yMax; y++ ) {
		// float jitter = blueNoiseRef( ivec2( x, y ) ).r / blueNoiseScale;
		// imageStore( target, ivec2( x, y ), vec4( col.rgb, col.a + jitter ) );
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
	float yBuffer			= 13.0f * hPixels / 24.0f;

	// no reason to run outside the width of the minimap
	if ( myXIndex > wPixels ) return;

	// initial clear
	DrawVerticalLine( myXIndex, 0, int( hPixels ), vec4( 0.0f ) );

	// FoV considerations
		// mapping [0..wPixels] to [-1..1]
	float FoVAdjust			= -1.0f + float( myXIndex ) * ( 2.0f ) / float( wPixels );

	const mat2 rotation		= Rotate2D( viewAngle + FoVAdjust * FoVScalar );
	const vec2 direction	= rotation * vec2( 1.0f, 0.0f );

	const vec2 forwards = globalForwards = rotate2D( viewAngle ) * vec2( 1.0f, 0.0f );
	const vec2 fixedDirection = direction * ( dot( direction, forwards ) / dot( forwards, forwards ) );

	vec3 sideVector			= cross( vec3( forwards.x, 0.0f, forwards.y ), vec3( 0.0f, 1.0f, 0.0f ) );
	vec2 viewPositionLocal	= startCenter + sideVector.xz * FoVAdjust * offsetScalar;

	// need a side-to-side adjust to give some spread - CPU side adjustment scalar needs to be added
	vec3 sideVector			= cross( vec3( direction.x, 0.0f, direction.y ), vec3( 0.0f, 1.0f, 0.0f ) );
	vec2 viewPositionLocal	= viewPosition + sideVector.xz * FoVAdjust * offsetScalar;

	// draw the vertical strip of pixels for myXIndex
	const float dz = 0.25f;
	for ( float dSample = 1.0f; dSample < maxDistance && yBuffer < hPixels; dSample += dz ) {
		const ivec2 samplePosition	= ivec2( viewPositionLocal + dSample * direction );
		const uvec4 dataSample		= imageLoad( map, samplePosition );
		const float heightOnScreen	= ( ( float( dataSample.a ) - viewerHeight ) * ( 1.0f / dSample ) * heightScalar + horizonLine );
		if ( heightOnScreen > yBuffer ) {
			uint depthTerm = uint( max( 0, 255 - dSample * fogScalar ) );
			DrawVerticalLine( myXIndex, int( yBuffer ), int( heightOnScreen ), vec4( dataSample.xyz, depthTerm ) / 255.0f );
			yBuffer = uint( heightOnScreen );
		}
	}
}
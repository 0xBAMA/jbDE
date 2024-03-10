#include "daedalus.h"

void Daedalus::ResetAccumulators() {
	ZoneScoped;
	textureManager.ZeroTexture2D( "Color Accumulator" );
	textureManager.ZeroTexture2D( "Depth/Normals Accumulator" );
	textureManager.ZeroTexture2D( "Tonemapped" );
	daedalusConfig.tiles.RegenerateTileList(); // timer / sample count reset
}

void Daedalus::ResizeAccumulators( uint32_t x, uint32_t y ) {
	ZoneScoped;
	// destroy the existing textures
	textureManager.Remove( "Depth/Normals Accumulator" );
	textureManager.Remove( "Color Accumulator Scratch" );
	textureManager.Remove( "Color Accumulator" );
	textureManager.Remove( "Tonemapped" );

	// create the new ones
	textureOptions_t opts;
	opts.dataType		= GL_RGBA32F;
	opts.width			= x;
	opts.height			= y;
	opts.minFilter		= GL_LINEAR;
	opts.magFilter		= GL_LINEAR;
	opts.textureType	= GL_TEXTURE_2D;
	opts.wrap			= GL_CLAMP_TO_BORDER;
	textureManager.Add( "Depth/Normals Accumulator", opts );
	textureManager.Add( "Color Accumulator Scratch", opts );
	textureManager.Add( "Color Accumulator", opts );
	textureManager.Add( "Tonemapped", opts );

	// regen the tile list, reset timer, etc
	daedalusConfig.targetWidth = x;
	daedalusConfig.targetHeight = y;
	daedalusConfig.tiles.Reset( daedalusConfig.tiles.tileSize, x, y );

	// recenter the view on the middle of the image
	daedalusConfig.view.outputOffset = daedalusConfig.view.outputZoom * ( vec2( daedalusConfig.targetWidth, daedalusConfig.targetHeight ) - vec2( config.width, config.height ) ) / 2.0f;
}

void Daedalus::SendSkyCacheUniforms() {
	const GLuint shader = shaders[ "Sky Cache" ];
	glUseProgram( shader );

	glUniform1f( glGetUniformLocation( shader, "skyTime" ), daedalusConfig.render.scene.skyTime );
	glUniform1i( glGetUniformLocation( shader, "mode" ), daedalusConfig.render.scene.skyMode );
	glUniform3fv( glGetUniformLocation( shader, "color1" ), 1, glm::value_ptr( daedalusConfig.render.scene.skyConstantColor1 ) );
	glUniform3fv( glGetUniformLocation( shader, "color2" ), 1, glm::value_ptr( daedalusConfig.render.scene.skyConstantColor2 ) );

	textureManager.BindImageForShader( "Sky Cache", "skyCache", shader, 0 );
	textureManager.BindImageForShader( "Blue Noise", "blueNoise", shader, 1 );
}

void Daedalus::SendBasePathtraceUniforms() {
	const GLuint shader = shaders[ "Pathtrace" ];
	glUseProgram( shader );

	static uint32_t sampleCount = 0;
	static ivec2 blueNoiseOffset;
	if ( sampleCount != daedalusConfig.tiles.SampleCount() ) sampleCount = daedalusConfig.tiles.SampleCount(),
		blueNoiseOffset = ivec2( daedalusConfig.rng.blueNoiseOffset(), daedalusConfig.rng.blueNoiseOffset() );
	glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), blueNoiseOffset.x, blueNoiseOffset.y );
	glUniform1i( glGetUniformLocation( shader, "subpixelJitterMethod" ), daedalusConfig.render.subpixelJitterMethod );
	glUniform1i( glGetUniformLocation( shader, "sampleNumber" ), daedalusConfig.tiles.SampleCount() );
	glUniform1i( glGetUniformLocation( shader, "bokehMode" ), daedalusConfig.render.bokehMode );
	glUniform1i( glGetUniformLocation( shader, "cameraType" ), daedalusConfig.render.cameraType );
	glUniform1f( glGetUniformLocation( shader, "uvScalar" ), daedalusConfig.render.uvScalar ); // consider making this 2d
	glUniform1f( glGetUniformLocation( shader, "exposure" ), daedalusConfig.render.exposure );
	glUniform1f( glGetUniformLocation( shader, "FoV" ), daedalusConfig.render.FoV );
	glUniform1f( glGetUniformLocation( shader, "maxDistance" ), daedalusConfig.render.maxDistance );
	glUniform1f( glGetUniformLocation( shader, "epsilon" ), daedalusConfig.render.epsilon );
	glUniform1i( glGetUniformLocation( shader, "maxBounces" ), daedalusConfig.render.maxBounces );
	glUniform1i( glGetUniformLocation( shader, "skyInvert" ), daedalusConfig.render.scene.skyInvert );
	glUniform1f( glGetUniformLocation( shader, "skyBrightnessScalar" ), daedalusConfig.render.scene.skyBrightnessScalar );

	glUniform1i( glGetUniformLocation( shader, "raymarchEnable" ), daedalusConfig.render.scene.raymarchEnable );
	glUniform1i( glGetUniformLocation( shader, "raymarchMaxSteps" ), daedalusConfig.render.scene.raymarchMaxSteps );
	glUniform1f( glGetUniformLocation( shader, "raymarchUnderstep" ), daedalusConfig.render.scene.raymarchUnderstep );
	glUniform1f( glGetUniformLocation( shader, "raymarchMaxDistance" ), daedalusConfig.render.scene.raymarchMaxDistance );

	glUniform1i( glGetUniformLocation( shader, "ddaSpheresEnable" ), daedalusConfig.render.scene.ddaSpheresEnable );
	glUniform1f( glGetUniformLocation( shader, "ddaSpheresBoundSize" ), daedalusConfig.render.scene.ddaSpheresBoundSize );
	glUniform1i( glGetUniformLocation( shader, "ddaSpheresResolution" ), daedalusConfig.render.scene.ddaSpheresResolution );

	glUniform3fv( glGetUniformLocation( shader, "viewerPosition" ), 1, glm::value_ptr( daedalusConfig.render.viewerPosition ) );
	glUniform3fv( glGetUniformLocation( shader, "basisX" ), 1, glm::value_ptr( daedalusConfig.render.basisX ) );
	glUniform3fv( glGetUniformLocation( shader, "basisY" ), 1, glm::value_ptr( daedalusConfig.render.basisY ) );
	glUniform3fv( glGetUniformLocation( shader, "basisZ" ), 1, glm::value_ptr( daedalusConfig.render.basisZ ) );
	glUniform1i( glGetUniformLocation( shader, "thinLensEnable" ), daedalusConfig.render.thinLensEnable );
	glUniform1f( glGetUniformLocation( shader, "thinLensFocusDistance" ), daedalusConfig.render.thinLensFocusDistance );
	glUniform1f( glGetUniformLocation( shader, "thinLensJitterRadius" ), daedalusConfig.render.thinLensJitterRadius );

	textureManager.BindImageForShader( "Blue Noise", "blueNoise", shader, 0 );
	textureManager.BindImageForShader( "Color Accumulator", "accumulatorColor", shader, 1 );
	textureManager.BindImageForShader( "Depth/Normals Accumulator", "accumulatorNormalsAndDepth", shader, 2 );
	textureManager.BindTexForShader( "Sky Cache", "skyCache", shader, 3 );

	
}

void Daedalus::SendInnerLoopPathtraceUniforms() {
	const GLuint shader = shaders[ "Pathtrace" ];
	const ivec2 tileOffset = daedalusConfig.tiles.GetTile(); // send uniforms ( unique per loop iteration )
	glUniform2i( glGetUniformLocation( shader, "tileOffset" ),	tileOffset.x, tileOffset.y );
	glUniform1i( glGetUniformLocation( shader, "wangSeed" ),	daedalusConfig.rng.wangSeeder() );
}

void Daedalus::SendPrepareUniforms() {
	const GLuint shader = shaders[ "Prepare" ];
	glUseProgram( shader );

	textureManager.BindImageForShader( "Blue Noise", "blueNoise", shader, 0 );
	textureManager.BindImageForShader( "Color Accumulator", "accumulatorColor", shader, 1 );
	textureManager.BindImageForShader( "Depth/Normals Accumulator", "accumulatorNormalsAndDepth", shader, 2 );
	textureManager.BindImageForShader( "Tonemapped", "tonemappedResult", shader, 3 );
}

void Daedalus::SendPresentUniforms() {
	const GLuint shader = shaders[ "Present" ];
	glUseProgram( shader );

	glUniform1f( glGetUniformLocation( shader, "scale" ), daedalusConfig.view.outputZoom );
	glUniform2f( glGetUniformLocation( shader, "offset" ), daedalusConfig.view.outputOffset.x, daedalusConfig.view.outputOffset.y );
	glUniform1i( glGetUniformLocation( shader, "edgeLines" ), daedalusConfig.view.edgeLines );
	glUniform3fv( glGetUniformLocation( shader, "edgeLinesColor" ), 1, glm::value_ptr( daedalusConfig.view.edgeLinesColor ) );
	glUniform1i( glGetUniformLocation( shader, "centerLines" ), daedalusConfig.view.centerLines );
	glUniform3fv( glGetUniformLocation( shader, "centerLinesColor" ), 1, glm::value_ptr( daedalusConfig.view.centerLinesColor ) );
	glUniform1i( glGetUniformLocation( shader, "ROTLines" ), daedalusConfig.view.ROTLines );
	glUniform3fv( glGetUniformLocation( shader, "ROTLinesColor" ), 1, glm::value_ptr( daedalusConfig.view.ROTLinesColor ) );
	glUniform1i( glGetUniformLocation( shader, "goldenLines" ), daedalusConfig.view.goldenLines );
	glUniform3fv( glGetUniformLocation( shader, "goldenLinesColor" ), 1, glm::value_ptr( daedalusConfig.view.goldenLinesColor ) );
	glUniform1i( glGetUniformLocation( shader, "vignette" ), daedalusConfig.view.vignette );

	textureManager.BindImageForShader( "Blue Noise", "blueNoise", shader, 0 );
	textureManager.BindTexForShader( "Tonemapped", "preppedImage", shader, 1 );
	textureManager.BindImageForShader( "Display Texture", "outputImage", shader, 2 );
}
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

void Daedalus::Screenshot( string label, bool srgbConvert, bool fullDepth ) {
	const GLuint tex = textureManager.Get( label );
	uvec2 dims = textureManager.GetDimensions( label );
	std::vector< float > imageBytesToSave;
	imageBytesToSave.resize( dims.x * dims.y * sizeof( float ) * 4, 0 );
	glBindTexture( GL_TEXTURE_2D, tex );
	glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
	Image_4F screenshot( dims.x, dims.y, &imageBytesToSave.data()[ 0 ] );
	if ( srgbConvert == true ) {
		screenshot.RGBtoSRGB();
	}
	const string filename = string( "Daedalus-" ) + timeDateString() + string( fullDepth ? ".exr" : ".png" );
	screenshot.Save( filename, fullDepth ? Image_4F::backend::TINYEXR : Image_4F::backend::LODEPNG );
}

void Daedalus::ApplyFilter( int mode, int count ) {
	rngi pick = rngi( 0, 3 );
	GLuint shader;
	const uvec2 dims = textureManager.GetDimensions( "Color Accumulator" );
	for ( int i = 0; i < count; i++ ) {
		shader = shaders[ "Filter" ];
		glUseProgram( shader );
		glUniform1i( glGetUniformLocation( shader, "mode" ), mode == 4 ? pick() : mode );
		textureManager.BindImageForShader( "Color Accumulator", "sourceData", shader, 0 );
		textureManager.BindImageForShader( "Color Accumulator Scratch", "destData", shader, 1 );
		glDispatchCompute( ( dims.x + 15 ) / 16, ( dims.y + 15 ) / 16, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		shader = shaders[ "Copy Back" ];
		glUseProgram( shader );
		textureManager.BindImageForShader( "Color Accumulator Scratch", "sourceData", shader, 0 );
		textureManager.BindImageForShader( "Color Accumulator", "destData", shader, 1 );
		glDispatchCompute( ( dims.x + 15 ) / 16, ( dims.y + 15 ) / 16, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}
}


void Daedalus::SendSkyCacheUniforms() {
	const GLuint shader = shaders[ "Sky Cache" ];
	glUseProgram( shader );

	glUniform1f( glGetUniformLocation( shader, "skyTime" ), daedalusConfig.render.scene.skyTime );
	glUniform1i( glGetUniformLocation( shader, "mode" ), daedalusConfig.render.scene.skyMode );
	glUniform3fv( glGetUniformLocation( shader, "color1" ), 1, glm::value_ptr( daedalusConfig.render.scene.skyConstantColor1 ) );
	glUniform3fv( glGetUniformLocation( shader, "color2" ), 1, glm::value_ptr( daedalusConfig.render.scene.skyConstantColor2 ) );
	glUniform1i( glGetUniformLocation( shader, "sunThresh" ), daedalusConfig.render.scene.sunThresh );

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
	glUniform1i( glGetUniformLocation( shader, "cameraOriginJitter" ), daedalusConfig.render.cameraOriginJitter );
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

	glUniform1i( glGetUniformLocation( shader, "maskedPlaneEnable" ), daedalusConfig.render.scene.maskedPlaneEnable );

	glUniform1i( glGetUniformLocation( shader, "numExplicitPrimitives" ), daedalusConfig.render.scene.numExplicitPrimitives );
	glUniform1i( glGetUniformLocation( shader, "explicitListEnable" ), daedalusConfig.render.scene.explicitListEnable );

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
	textureManager.BindImageForShader( "TextBuffer", "textBuffer", shader, 4 );
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

void Daedalus::PrepSphereBufferRandom() {
	// clear it out
	daedalusConfig.render.scene.explicitPrimitiveData.clear();
	palette::PickRandomPalette( true );

	// simple implementation, randomizing all parameters
	rng c = rng(  0.3f, 1.0f );
	rng o = rng( -1.0f, 1.0f );
	rng r = rng(  0.1f, 0.4f );
	rng IoR = rng( 1.0f / 1.1f, 1.0f / 1.5f );
	rng Roughness = rng( 0.0f, 0.001f );

	for ( int idx = 0; idx < daedalusConfig.render.scene.numExplicitPrimitives; idx++ ) {
		vec3 color = palette::paletteRef( c() );
		daedalusConfig.render.scene.explicitPrimitiveData.push_back( vec4( o(), o(), o(), r() ) );				// position
		daedalusConfig.render.scene.explicitPrimitiveData.push_back( vec4( color.x, color.y, color.z, 15.0f ) );	// color
		daedalusConfig.render.scene.explicitPrimitiveData.push_back( vec4( IoR(), Roughness(), 0.0f, 0.0f ) );	// material properties
	}
}

void Daedalus::PrepSphereBufferPacking() {
	// TODO

	// // stochastic sphere packing, inside the volume
	// vec3 min = vec3( -8.0f, -1.5f, -8.0f );
	// vec3 max = vec3(  8.0f,  1.5f,  8.0f );
	// uint32_t maxIterations = 500;
	// float currentRadius = 1.3f;
	// rng paletteRefVal = rng( 0.0f, 1.0f );
	// int material = 6;
	// vec4 currentMaterial = vec4( palette::paletteRef( paletteRefVal() ), material );
	// while ( ( sirenConfig.sphereLocationsPlusColors.size() / 2 ) < sirenConfig.maxSpheres ) {
	// 	rng x = rng( min.x + currentRadius, max.x - currentRadius );
	// 	rng y = rng( min.y + currentRadius, max.y - currentRadius );
	// 	rng z = rng( min.z + currentRadius, max.z - currentRadius );
	// 	uint32_t iterations = maxIterations;
	// 	// redundant check, but I think it's the easiest way not to run over
	// 	while ( iterations-- && ( sirenConfig.sphereLocationsPlusColors.size() / 2 ) < sirenConfig.maxSpheres ) {
	// 		// generate point inside the parent cube
	// 		vec3 checkP = vec3( x(), y(), z() );
	// 		// check for intersection against all other spheres
	// 		bool foundIntersection = false;
	// 		for ( uint idx = 0; idx < sirenConfig.sphereLocationsPlusColors.size() / 2; idx++ ) {
	// 			vec4 otherSphere = sirenConfig.sphereLocationsPlusColors[ 2 * idx ];
	// 			if ( glm::distance( checkP, otherSphere.xyz() ) < ( currentRadius + otherSphere.a ) ) {
	// 				// cout << "intersection found in iteration " << iterations << endl;
	// 				foundIntersection = true;
	// 				break;
	// 			}
	// 		}
	// 		// if there are no intersections, add it to the list with the current material
	// 		if ( !foundIntersection ) {
	// 			// cout << "adding sphere, " << currentRadius << endl;
	// 			sirenConfig.sphereLocationsPlusColors.push_back( vec4( checkP, currentRadius ) );
	// 			sirenConfig.sphereLocationsPlusColors.push_back( currentMaterial );
	// 			cout << "\r" << fixedWidthNumberString( sirenConfig.sphereLocationsPlusColors.size() / 2, 6, ' ' ) << " / " << sirenConfig.maxSpheres << " after " << Tock() / 1000.0f << "s                          ";
	// 		}
	// 	}
	// 	// if you've gone max iterations, time to halve the radius and double the max iteration count, get new material
	// 		currentMaterial = vec4( palette::paletteRef( paletteRefVal() ), material );
	// 		currentRadius /= 1.618f;
	// 		maxIterations *= 3;

	// 		// doing this makes it pack flat
	// 		// min.y /= 1.5f;
	// 		// max.y /= 1.5f;

	// 		// slowly shrink bounds to accentuate the earlier placed spheres
	// 		min *= 0.95f;
	// 		max *= 0.95f;
	// }

}

void Daedalus::RelaxSphereList() {
	// this only applies to spheres... whatever
	rng o = rng( -0.1f, 0.1f );
	int iterations = 0;
	const int maxIterations = 100;
	while ( 1 ) { // relaxation step
		// walk the list, repulstion force between pairs
		for ( int i = 0; i < daedalusConfig.render.scene.numExplicitPrimitives; i++ ) {
			for ( int j = i + 1; j < daedalusConfig.render.scene.numExplicitPrimitives; j++ ) {
				// sphere i and sphere j move slightly away from one another if intersecting
				vec4 sphereI = daedalusConfig.render.scene.explicitPrimitiveData[ 3 * i ];
				vec4 sphereJ = daedalusConfig.render.scene.explicitPrimitiveData[ 3 * j ];
				float combinedRadius = sphereI.w + sphereJ.w;
				vec3 displacement = sphereI.xyz() - sphereJ.xyz();
				float lengthDisplacement = glm::length( displacement );
				if ( lengthDisplacement < combinedRadius ) {
					const float offset = combinedRadius - lengthDisplacement;
					daedalusConfig.render.scene.explicitPrimitiveData[ 3 * i ] = glm::vec4( sphereI.xyz() + ( offset / 2.0f + o() ) * glm::normalize( displacement ), sphereI.w );
					daedalusConfig.render.scene.explicitPrimitiveData[ 3 * j ] = glm::vec4( sphereJ.xyz() - ( offset / 2.0f + o() ) * glm::normalize( displacement ), sphereJ.w );
				}
			}
		}
		// break out when there are no intersections
		bool existsIntersections = false;
		for ( int i = 0; i < daedalusConfig.render.scene.numExplicitPrimitives; i++ ) {
			for ( int j = i + 1; j < daedalusConfig.render.scene.numExplicitPrimitives; j++ ) {
				vec4 sphereI = daedalusConfig.render.scene.explicitPrimitiveData[ 3 * i ];
				vec4 sphereJ = daedalusConfig.render.scene.explicitPrimitiveData[ 3 * j ];
				float combinedRadius = sphereI.w + sphereJ.w;
				vec3 displacement = sphereI.xyz() - sphereJ.xyz();
				if ( glm::length( displacement ) < combinedRadius ) {
					existsIntersections = true;
				}
			}
		}
		// we have made sure nobody intersects, or something is hanging
		if ( !existsIntersections || ( iterations++ == maxIterations ) ) break;
	}
}

void Daedalus::SendExplicitPrimitiveSSBO() {
	// send the data from the vector to the GPU
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, daedalusConfig.render.scene.explicitPrimitiveSSBO );
	glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 4 * 3 * daedalusConfig.render.scene.numExplicitPrimitives, ( GLvoid * ) &daedalusConfig.render.scene.explicitPrimitiveData[ 0 ], GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, daedalusConfig.render.scene.explicitPrimitiveSSBO );
}

void Daedalus::PrepGlyphBuffer() {
	// block dimensions
	uint32_t texW = 96;
	uint32_t texH = 64;
	uint32_t texD = 32;

	texW = 256;
	texH = 256;
	texD = 1024;

	Image_4U data( texW, texH * texD );

	// create the voxel model inside the flat image
		// update the data
	const uint32_t numOps = 0;
	rngi opPick = rngi( 0, 34 );
	rngi xPick = rngi( 0, texW - 1 ); rngi xDPick = rngi( 4, 20 );
	rngi yPick = rngi( 0, texH - 1 ); rngi yDPick = rngi( 4, 12 );
	rngi zPick = rngi( 0, texD - 1 ); rngi zDPick = rngi( 4, 10 );
	rngi glyphPick = rngi( 176, 223 );
	rngi thinPick = rngi( 0, 4 );

	// look at a random, narrow slice of a random palette
	palette::PickRandomPalette( true );
	rng ppPick = rng( 0.1f, 0.9f );
	rng pwPick = rng( 0.01f, 0.3f );
	rngN pPick = rngN( ppPick(), pwPick() );

	color_4U col = color_4U( { 0u, 0u, 0u, 1u } );
	uint32_t length = 0;
	PerlinNoise p;

	rng noiseScale = rng( 10.0f, 50.0f );
	const vec3 n = vec3( noiseScale(), noiseScale(), 10.0f * noiseScale() );

	rng offset = rng( -10.0f, 10.0f );
	const vec3 o = vec3( offset(), offset(), offset() );

	auto fnSimplex = FastNoise::New<FastNoise::Simplex>();
	auto fnFractal = FastNoise::New<FastNoise::FractalFBm>();
	fnFractal->SetSource( fnSimplex );
	fnFractal->SetOctaveCount( 5 );

	rngi noiseSeedGen = rngi( 0, 1000000 );
	int noiseSeed = noiseSeedGen();
	int noiseSeed2 = noiseSeedGen();

	for ( uint32_t x = 0; x < texW; x++ )
	for ( uint32_t y = 0; y < texH; y++ )
	for ( uint32_t z = 0; z < texD; z++ ) {
		// float d = glm::distance( vec3( texW / 2.0f, texH / 2.0f, texD / 2.0f ), vec3( x, y, z ) );
		// float d2 = glm::distance( vec2( texW / 2.0f, texH / 2.0f ), vec2( x, y ) );
		// if ( d < 64.0f && d2 > 26.0f || d2 < 22.0f && d2 > 12.0f ) {

			// float noiseValue = p.noise( x / n.x + o.x, y / n.y + o.y, z / n.z + o.z );
			// if ( noiseValue < 0.5f ) {
				// vec3 c = palette::paletteRef( pPick() ) * 255.0f;
				// col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
				// vec3 c = palette::paletteRef( noiseValue ) * 255.0f;
				// col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
				// data.SetAtXY( x, y + z * texH, col );
			// }
		// }

		// frame test
		const ivec3 numBeams = ivec3( 2, 2, 4 );
		const ivec3 beamWidths = ivec3( 12, 16, 46 );
		const bool xValid = ( x % ( ( texW - beamWidths.x / 2 ) / numBeams.x ) < beamWidths.x );
		const bool yValid = ( y % ( ( texH - beamWidths.y / 2 ) / numBeams.y ) < beamWidths.y );
		const bool zValid = ( z % ( ( texD - beamWidths.z / 2 ) / numBeams.z ) < beamWidths.z );
		if ( ( xValid && yValid ) || ( yValid && zValid ) || ( xValid && zValid ) ) {
			// vec3 c = palette::paletteRef( 0.5f );
			// col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
			// col = color_4U( { 0u, 128u, 0u, ( uint8_t ) glyphPick() } );
			vec3 c = palette::paletteRef( pPick() ) * 255.0f;
			col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
			data.SetAtXY( x, y + z * texH, col );
		}

		// // noise test
		// if ( fnFractal->GenSingle3D( x / 40.0f, y / 40.0f, z / 500.0f, noiseSeed ) < -0.4f ) {
		// 	vec3 c = palette::paletteRef( pPick() ) * 255.0f;
		// 	col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
		// 	data.SetAtXY( x, y + z * texH, col );
		// }

	}

	palette::PickRandomPalette( true );

	std::vector< float > noiseOutput( texW * texH * texD );
	std::vector< float > noiseOutput2( texW * texH * texD );
	// std::vector< float > noiseOutput3( texD * texH );
	fnFractal->GenUniformGrid3D( noiseOutput.data(), 0, 0, 0, texD, texH, texW, 0.01f, noiseSeed );
	fnFractal->GenUniformGrid3D( noiseOutput2.data(), 0, 0, 0, texW, texH, texD, 0.005f, noiseSeed2 );
	// fnFractal->GenUniformGrid2D( noiseOutput3.data(), 0, 0, texD, texH, 0.01f, noiseSeed2 );
	int index = 0;
	// int index2 = 0;
	for ( uint32_t x = 0; x < texW; x++ ) {
		for ( uint32_t y = 0; y < texH; y++ ) {
			float heightmapRead = fnFractal->GenSingle3D( x / 120.0f, y / 120.0f, 10.0f, noiseSeed ) * 0.5f + 0.45f;
			for ( uint32_t z = 0; z < texD; z++ ) {
				// noise test
				int i = index++;
				float noiseValue = noiseOutput[ i ];
				float noiseValue2 = noiseOutput2[ i ];
				// if ( noiseValue < -0.4f ) {
				// if ( ( z / texD ) < heightmapRead ) {
				if ( float( z ) / float( texD ) < heightmapRead && noiseValue < -0.3f ) {
					// vec3 c = palette::paletteRef( abs( noiseValue2 ) ) * 255.0f;
					vec3 c = palette::paletteRef( abs( heightmapRead ) ) * 255.0f;
					col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
					// col = color_4U( { ( uint8_t ) heightmapRead * 255, ( uint8_t ) heightmapRead * 255, ( uint8_t ) heightmapRead * 255, ( uint8_t ) glyphPick() } );
					data.SetAtXY( x, y + z * texH, col );
				}
			}
		}
	}

	// beam/block test
	// palette::PickRandomPalette( true );
	// // rngi xD = rngi( texH / 2.0f - 50.0f, texH / 2.0f + 50.0f );
	// // rngi yD = rngi( texW / 2.0f - 50.0f, texW / 2.0f + 50.0f );
	// // rngi zD = rngi( texD / 2.0f - 50.0f, texD / 2.0f + 50.0f );
	// rngN xD = rngN( texH / 2.0f, 20.0f );
	// rngN yD = rngN( texW / 2.0f, 20.0f );
	// rngN zD = rngN( texD / 2.0f, 20.0f );
	// for ( int i = 0; i < 400000; i++ ) {
	// 	vec3 c = palette::paletteRef( pPick() ) * 255.0f;
	// 	col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
	// 	data.SetAtXY( uint32_t( xD() ), uint32_t( yD() ) + uint32_t( zD() ) * texD, col );
	// }

	// swatch test
	// palette::PickRandomPalette( true );
	// for ( uint x = 0; x < 16; x++ )
	// for ( uint y = 0; y < 16; y++ ) {
	// 	rngN dist = rngN( 0.0, 2.0f );
	// 	rngN distL = rngN( 0.0, 20.0f );
	// 	for ( int i = 0; i < 1000; i++ ) {
	// 		vec3 c = palette::paletteRef( ( x + 16 * y ) / 255.0f ) * 255.0f;
	// 		col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) ( x + 16 * y ) } );
	// 		data.SetAtXY( uint32_t( x * 16.0f + dist() + 8 ), uint32_t( y * 16.0f + dist() + 8 ) + uint32_t( distL() + 128 ) * texD, col );
	// 	}
	// }

	// do N randomly selected
	for ( uint32_t op = 0; op < numOps; op++ ) {
		vec3 c = palette::paletteRef( pPick() ) * 255.0f;
		switch ( opPick() ) {
		case 0: // AABB
		case 1:
		{
			// col = color_4U( { 255u, 255u, 255u, ( uint8_t ) glyphPick() } );
			col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );

			uvec3 base = uvec3( xPick(),  yPick(),  zPick() );
			uvec3 dims = uvec3( xDPick(), yDPick(), zDPick() );
			for ( uint32_t x = base.x; x < base.x + dims.x; x++ )
			for ( uint32_t y = base.y; y < base.y + dims.y; y++ )
			for ( uint32_t z = base.z; z < base.z + dims.z; z++ )
				data.SetAtXY( x, y + z * texH, col );
			break;
		}

		case 2: // lines
		case 3:
		case 4:
		case 5:
		case 6:
		{
			col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
			uvec3 base = uvec3( xPick(), yPick(), zPick() );
			length = xDPick();
			for ( uint32_t x = base.x; x < base.x + length; x++ )
				data.SetAtXY( x, base.y + base.z * texH, col );
			break;
		}

		case 7: // other lines
		case 8:
		case 9:
		case 10:
		case 11:
		{
			col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
			uvec3 base = uvec3( xPick(), yPick(), zPick() );
			length = yDPick();
			for ( uint32_t y = base.y; y < base.y + length; y++ )
				data.SetAtXY( base.x, y + base.z * texH, col );
			break;
		}

		case 12: // more other-er lines
		case 13:
		case 14:
		case 15:
		case 16:
		case 17:
		{
			col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );
			uvec3 base = uvec3( xPick(), yPick(), zPick() );
			length = zDPick();
			for ( uint32_t z = base.z; z < base.z + length; z++ )
				data.SetAtXY( base.x, base.y + z * texH, col );
			break;
		}

		case 18: // should come out emissive
		case 20:
		case 21:
		{
			col = color_4U( { 255u, 255u, 255u, ( uint8_t ) glyphPick() } );
			// col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) glyphPick() } );

			uvec3 base = uvec3( xPick(), yPick(), zPick() );
			uvec3 dims = uvec3( thinPick() + 1, thinPick() + 1, thinPick() + 1 );
			for ( uint32_t x = base.x; x < base.x + dims.x; x++ )
			for ( uint32_t y = base.y; y < base.y + dims.y; y++ )
			for ( uint32_t z = base.z; z < base.z + dims.z; z++ )
				data.SetAtXY( x, y + z * texH, col );
			break;
		}

		case 22: // string text
		case 23:
		case 24:
		case 25:
		case 26:
		{
			static rngi wordPick = rngi( 0, colorWords.size() - 1 );
			string word = colorWords[ wordPick() ];
			uvec3 basePt = uvec3( xPick(), yPick(), zPick() );
			for ( size_t i = 0; i < word.length(); i++ ) {
				col = color_4U( { ( uint8_t ) c.x, ( uint8_t ) c.y, ( uint8_t ) c.z, ( uint8_t ) word[ i ] } );
				data.SetAtXY( basePt.x + i, basePt.y + basePt.z * texH, col );
			}
			break;
		}

		case 27:
		case 28:
		case 29:
		case 30:
		case 31:
		case 32:
		case 33:
		case 34:
		{	// clear out an AABB
			col = color_4U( { 0u, 0u, 0u, 0u } );
			uvec3 base = uvec3( xPick(),  yPick(),  zPick() );
			uvec3 dims = uvec3( xDPick(), yDPick(), zDPick() );
			for ( uint32_t x = base.x; x < base.x + dims.x; x++ )
			for ( uint32_t y = base.y; y < base.y + dims.y; y++ )
			for ( uint32_t z = base.z; z < base.z + dims.z; z++ )
				data.SetAtXY( x, y + z * texH, col );
			break;
		}

		case 35:
		{
			col = color_4U( { 255u, 255u, 255u, ( uint8_t ) glyphPick() } );
			ivec3 base = ivec3( xPick(),  yPick(),  zPick() );
			data.SetAtXY( base.x, base.y + base.z * texH, col );
			break;
		}

		default:
			break;
		}
	}

	// texture is ready to be used in the pathtrace
		// create a texture, and send the data
	static bool firstRun = true;
	if ( firstRun ) {
		firstRun = false;
		textureOptions_t opts;
		opts.width			= texW;
		opts.height			= texH;
		opts.depth			= texD;
		opts.dataType		= GL_RGBA8UI;
		opts.minFilter		= GL_NEAREST;
		opts.magFilter		= GL_NEAREST;
		opts.textureType	= GL_TEXTURE_3D;
		opts.wrap			= GL_CLAMP_TO_BORDER;
		opts.initialData = ( void * ) data.GetImageDataBasePtr();
		textureManager.Add( "TextBuffer", opts );
	} else {
		// pass the new generated texture data to the existing texture
		glBindTexture( GL_TEXTURE_3D, textureManager.Get( "TextBuffer" ) );
		glTexImage3D( GL_TEXTURE_3D, 0, GL_RGBA8UI, texW, texH, texD, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, ( void * ) data.GetImageDataBasePtr() );
	}
}
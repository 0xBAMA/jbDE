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

	for ( int idx = 0; idx < daedalusConfig.render.scene.numExplicitPrimitives; idx++ ) {
		vec3 color = palette::paletteRef( c() );
		daedalusConfig.render.scene.explicitPrimitiveData.push_back( vec4( o(), o(), o(), r() ) );				// position
		daedalusConfig.render.scene.explicitPrimitiveData.push_back( vec4( color.x, color.y, color.z, 15.0f ) );	// color
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
				vec4 sphereI = daedalusConfig.render.scene.explicitPrimitiveData[ 2 * i ];
				vec4 sphereJ = daedalusConfig.render.scene.explicitPrimitiveData[ 2 * j ];
				float combinedRadius = sphereI.w + sphereJ.w;
				vec3 displacement = sphereI.xyz() - sphereJ.xyz();
				float lengthDisplacement = glm::length( displacement );
				if ( lengthDisplacement < combinedRadius ) {
					const float offset = combinedRadius - lengthDisplacement;
					daedalusConfig.render.scene.explicitPrimitiveData[ 2 * i ] = glm::vec4( sphereI.xyz() + ( offset / 2.0f + o() ) * glm::normalize( displacement ), sphereI.w );
					daedalusConfig.render.scene.explicitPrimitiveData[ 2 * j ] = glm::vec4( sphereJ.xyz() - ( offset / 2.0f + o() ) * glm::normalize( displacement ), sphereJ.w );
				}
			}
		}
		// break out when there are no intersections
		bool existsIntersections = false;
		for ( int i = 0; i < daedalusConfig.render.scene.numExplicitPrimitives; i++ ) {
			for ( int j = i + 1; j < daedalusConfig.render.scene.numExplicitPrimitives; j++ ) {
				vec4 sphereI = daedalusConfig.render.scene.explicitPrimitiveData[ 2 * i ];
				vec4 sphereJ = daedalusConfig.render.scene.explicitPrimitiveData[ 2 * j ];
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
	glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * daedalusConfig.render.scene.numExplicitPrimitives, ( GLvoid * ) &daedalusConfig.render.scene.explicitPrimitiveData[ 0 ], GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, daedalusConfig.render.scene.explicitPrimitiveSSBO );
}

void Daedalus::PrepGlyphBuffer() {

}
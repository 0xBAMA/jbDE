#include "../../../engine/engine.h"

struct sirenConfig_t {
// program parameters and state

	// performance settings / monitoring
	uint32_t performanceHistorySamples;
	std::deque< float > fpsHistory;
	std::deque< float > tileHistory;
	uint32_t numFullscreenPasses = 0;
	int32_t sampleCountCap;				// -1 for unlimited

	// renderer state
	uint32_t tileSize;
	uint32_t targetWidth;
	uint32_t targetHeight;
	uint32_t tilePerFrameCap;
	// I think it would make sense to dispatch some N tiles between timer queries, to make better use of the GPU
		// probably configure that here

	bool tileListNeedsUpdate = true;	// need to generate new tile list ( if e.g. tile size changes )
	std::vector< ivec2 > tileOffsets;	// shuffled list of tiles
	uint32_t tileOffset = 0;			// offset into tile list - start at first element

	ivec2 blueNoiseOffset;
	float exposure;
	float renderFoV;
	vec3 viewerPosition;				// orientation will come from the trident, I think
	rngi wangSeeder = rngi( 0, 100000000 );	// initial value for the wang ( +x,y offset per invocation )

	// enhanced sphere tracing paper - potential for some optimizations - over-relaxation, refraction, dynamic epsilon
		// https://erleuchtet.org/~cupe/permanent/enhanced_sphere_tracing.pdf

	// raymarch parameters
	uint32_t raymarchMaxSteps;
	uint32_t raymarchMaxBounces;
	float raymarchMaxDistance;
	float raymarchEpsilon;
	float raymarchUnderstep;

// questionable need:
	// dither parameters ( mode, colorspace, pattern )
		// dithering the pathtrace result, seems, questionable - tbd
	// depth fog parameters ( mode, scalar )
		// will probably add this in
		// additionally, with depth + normals stored, SSAO? might add something, but will have to evaluate
	// display mode ( preview depth, normals, colors, pathtrace )
	// thin lens parameters ( focus distance, disk offset jitter )
	// normal mode - I think this doesn't really make sense to include, because only one really worked correctly last time

};

class Siren : public engineBase {	// example derived class
public:
	Siren () { Init(); OnInit(); PostInit(); }
	~Siren () { Quit(); }

	sirenConfig_t sirenConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			shaders[ "Pathtrace" ] = computeShader( "./src/projects/PathTracing/Siren/shaders/pathtrace.cs.glsl" ).shaderHandle;
			shaders[ "Postprocess" ] = computeShader( "./src/projects/PathTracing/Siren/shaders/postprocess.cs.glsl" ).shaderHandle;

			json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();
			sirenConfig.targetWidth					= j[ "app" ][ "Siren" ][ "targetWidth" ];
			sirenConfig.targetHeight				= j[ "app" ][ "Siren" ][ "targetHeight" ];
			sirenConfig.tileSize					= j[ "app" ][ "Siren" ][ "tileSize" ];
			sirenConfig.tilePerFrameCap				= j[ "app" ][ "Siren" ][ "tilePerFrameCap" ];
			sirenConfig.performanceHistorySamples	= j[ "app" ][ "Siren" ][ "performanceHistorySamples" ];
			sirenConfig.raymarchMaxSteps			= j[ "app" ][ "Siren" ][ "raymarchMaxSteps" ];
			sirenConfig.raymarchMaxBounces			= j[ "app" ][ "Siren" ][ "raymarchMaxBounces" ];
			sirenConfig.raymarchMaxDistance			= j[ "app" ][ "Siren" ][ "raymarchMaxDistance" ];
			sirenConfig.raymarchEpsilon				= j[ "app" ][ "Siren" ][ "raymarchEpsilon" ];
			sirenConfig.raymarchUnderstep			= j[ "app" ][ "Siren" ][ "raymarchUnderstep" ];
			sirenConfig.exposure					= j[ "app" ][ "Siren" ][ "exposure" ];
			sirenConfig.renderFoV					= j[ "app" ][ "Siren" ][ "renderFoV" ];
			sirenConfig.viewerPosition.x			= j[ "app" ][ "Siren" ][ "viewerPosition" ][ "x" ];
			sirenConfig.viewerPosition.y			= j[ "app" ][ "Siren" ][ "viewerPosition" ][ "y" ];
			sirenConfig.viewerPosition.z			= j[ "app" ][ "Siren" ][ "viewerPosition" ][ "z" ];

			// remove the 16-bit accumulator, because we're going to want a 32-bit version for this
			textureManager.Remove( "Accumulator" );

			// create the new accumulator(s)
			textureOptions_t opts;
			opts.dataType		= GL_RGBA32F;
			opts.width			= sirenConfig.targetWidth;
			opts.height			= sirenConfig.targetHeight;
			opts.minFilter		= GL_LINEAR;
			opts.magFilter		= GL_LINEAR;
			opts.textureType	= GL_TEXTURE_2D;

			textureManager.Add( "Depth/Normals Accumulator", opts );
			textureManager.Add( "Color Accumulator", opts );
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );
		// const uint8_t * state = SDL_GetKeyboardState( NULL );

	}

	void ImguiPass () {
		ZoneScoped;

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		QuitConf( &quitConfirm ); // show quit confirm window, ief triggered

	}

	void ScreenShots ( const bool colorEXR = true, const bool normalEXR = false, const bool tonemappedResult = false ) {
		if ( colorEXR == true ) {
			std::vector< uint8_t > imageBytesToSave;
			imageBytesToSave.resize( sirenConfig.targetWidth * sirenConfig.targetHeight * 4, 0 );
			// glBindTexture( GL_TEXTURE_2D, textures[ "Color Accumulator" ] );
			glBindTexture( GL_TEXTURE_2D, textures[ "Display Texture" ] );
			glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, &imageBytesToSave.data()[ 0 ] );
			Image_4U screenshot( sirenConfig.targetWidth, sirenConfig.targetHeight, &imageBytesToSave.data()[ 0 ] );

			// time string
			const string filename = string( "test" ) + timeDateString() + string( ".png" );

			// screenshot.FlipVertical();
			// screenshot.Resize( resize );
			// screenshot.Save( ssA.str(), Image_4F::backend::TINYEXR );
			screenshot.Save( filename );
			cout << "saved " << filename << endl;
		}
		if ( normalEXR == true ) {

		}
		if ( tonemappedResult == true ) {

		}
	}

	void ComputePasses () {
		ZoneScoped;

		{
			scopedTimer Start( "Tiled Update" );
			const GLuint shader = shaders[ "Pathtrace" ];
			glUseProgram( shader );

			// send uniforms ( initial, shared across frame )
			glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), sirenConfig.blueNoiseOffset.x, sirenConfig.blueNoiseOffset.y );
			glUniform1i( glGetUniformLocation( shader, "raymarchMaxSteps" ), sirenConfig.raymarchMaxSteps );
			glUniform1i( glGetUniformLocation( shader, "raymarchMaxBounces" ), sirenConfig.raymarchMaxBounces );
			glUniform1i( glGetUniformLocation( shader, "raymarchMaxDistance" ), sirenConfig.raymarchMaxDistance );
			glUniform1i( glGetUniformLocation( shader, "raymarchEpsilon" ), sirenConfig.raymarchEpsilon );
			glUniform1i( glGetUniformLocation( shader, "raymarchUnderstep" ), sirenConfig.raymarchUnderstep );
			glUniform1f( glGetUniformLocation( shader, "exposure" ), sirenConfig.exposure );
			glUniform1f( glGetUniformLocation( shader, "FoV" ), sirenConfig.renderFoV );
			glUniform3fv( glGetUniformLocation( shader, "viewerPosition" ), 1, glm::value_ptr( sirenConfig.viewerPosition ) );
			glUniform3fv( glGetUniformLocation( shader, "basisX" ), 1, glm::value_ptr( trident.basisX ) );
			glUniform3fv( glGetUniformLocation( shader, "basisY" ), 1, glm::value_ptr( trident.basisY ) );
			glUniform3fv( glGetUniformLocation( shader, "basisZ" ), 1, glm::value_ptr( trident.basisZ ) );

			// send uniforms ( per loop iteration )
			ivec2 tileOffset = GetTile();
			glUniform2i( glGetUniformLocation( shader, "tileOffset" ), tileOffset.x, tileOffset.y );
			glUniform1i( glGetUniformLocation( shader, "wangSeed" ), sirenConfig.wangSeeder() );

			textureManager.BindImageForShader( "Color Accumulator", "colorAccumulator", shader, 0 );
			textureManager.BindImageForShader( "Depth/Normals Accumulator", "depthAccumulator", shader, 1 );
			textureManager.BindImageForShader( "Blue Noise", "blueNoise", shader, 2 );

			// going to basically say that tilesizes are multiples of 16
			glDispatchCompute( sirenConfig.tileSize / 16, sirenConfig.tileSize / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // postprocessing - shader for color grading ( color temp, contrast, gamma ... ) + tonemapping
			// potentially add stuff like SSAO, depth fog, etc
			scopedTimer Start( "Postprocess" );
			const GLuint shader = shaders[ "Postprocess" ];
			glUseProgram( shader );

			textureManager.BindTexForShader( "Color Accumulator", "source", shader, 0 );
			textureManager.BindImageForShader( "Display Texture", "displayTexture", shader, 1 );

			// eventually more will be going on with this pass ( dither, depth fog, SSAO, etc )
			static float prevColorTemperature = 0.0f;
			static vec3 temperatureColor;
			if ( tonemap.colorTemp != prevColorTemperature ) {
				prevColorTemperature = tonemap.colorTemp;
				temperatureColor = GetColorForTemperature( tonemap.colorTemp );
			}
			glUniform3fv( glGetUniformLocation( shader, "colorTempAdjust" ), 1, glm::value_ptr( temperatureColor ) );
			glUniform1i( glGetUniformLocation( shader, "tonemapMode" ), tonemap.tonemapMode );
			glUniform1f( glGetUniformLocation( shader, "gamma" ), tonemap.gamma );
			glUniform2f( glGetUniformLocation( shader, "resolution" ), config.width, config.height );

			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );
			textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // show trident with current orientation
			scopedTimer Start( "Trident" );
			trident.Update( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void ResetAccumulators () {
		// clear the buffers
		Image_4U zeroes( sirenConfig.targetWidth, sirenConfig.targetHeight );
		GLuint handle = textureManager.Get( "Color Accumulator" );
		glBindTexture( GL_TEXTURE_2D, handle );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, zeroes.Width(), zeroes.Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, ( void * ) zeroes.GetImageDataBasePtr() );
		handle = textureManager.Get( "Depth/Normals Accumulator" );
		glBindTexture( GL_TEXTURE_2D, handle );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, zeroes.Width(), zeroes.Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, ( void * ) zeroes.GetImageDataBasePtr() );
	}

	void UpdateNoiseOffset () {
		static rng offsetGenerator = rng( 0, 512 );
		sirenConfig.blueNoiseOffset = ivec2( offsetGenerator(), offsetGenerator() );
	}

	ivec2 GetTile () {
		if ( sirenConfig.tileListNeedsUpdate == true ) {
			// construct the tile list ( runs at frame 0 and again any time the tilesize changes )
			sirenConfig.tileListNeedsUpdate = false;
			for ( int x = 0; x <= config.width; x += sirenConfig.tileSize ) {
				for ( int y = 0; y <= config.height; y += sirenConfig.tileSize ) {
					sirenConfig.tileOffsets.push_back( ivec2( x, y ) );
				}
			}
		} else { // check if the offset needs to be reset, this means a full pass has been completed
			if ( ++sirenConfig.tileOffset == sirenConfig.tileOffsets.size() ) {
				sirenConfig.tileOffset = 0;
				sirenConfig.numFullscreenPasses++;
			}
		}
		// shuffle when listOffset is zero ( first iteration, and any subsequent resets )
		if ( !sirenConfig.tileOffset ) {
			std::random_device rd;
			std::mt19937 rngen( rd() );
			std::shuffle( sirenConfig.tileOffsets.begin(), sirenConfig.tileOffsets.end(), rngen );
			// UpdateNoiseOffset(); // this pass's blue noise offset
			ScreenShots();
		}
		return sirenConfig.tileOffsets[ sirenConfig.tileOffset ];
	}


	void OnRender () {
		ZoneScoped;
		ClearColorAndDepth();		// if I just disable depth testing, this can disappear
		ComputePasses();			// multistage update of displayTexture
		BlitToScreen();				// fullscreen triangle copying to the screen
		{
			scopedTimer Start( "ImGUI Pass" );
			ImguiFrameStart();		// start the imgui frame
			ImguiPass();			// do all the gui stuff
			ImguiFrameEnd();		// finish imgui frame and put it in the framebuffer
		}
		window.Swap();				// show what has just been drawn to the back buffer
	}

	bool MainLoop () { // this is what's called from the loop in main
		ZoneScoped;

		// event handling
		HandleTridentEvents();
		HandleCustomEvents();
		HandleQuitEvents();

		// derived-class-specific functionality
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	Siren sirenInstance;
	while( !sirenInstance.MainLoop() );
	return 0;
}

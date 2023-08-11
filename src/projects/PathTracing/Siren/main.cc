#include "../../../engine/engine.h"

struct sirenConfig_t {
// program parameters and state

	// performance settings / monitoring
	uint32_t performanceHistorySamples;
	std::deque< float > timeHistory;	// ms per frame
	std::deque< float > tileHistory;	// completed tiles per frame
	uint32_t numFullscreenPasses = 0;
	int32_t sampleCountCap;				// -1 for unlimited

	// renderer state
	uint32_t tileSize;
	uint32_t targetWidth;
	uint32_t targetHeight;
	uint32_t tilesBetweenQueries;
	float tilesMSLimit;
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
	// thin lens parameters ( focus distance, disk offset jitter scale )
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

			ReloadShaders();

			json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();
			sirenConfig.targetWidth					= j[ "app" ][ "Siren" ][ "targetWidth" ];
			sirenConfig.targetHeight				= j[ "app" ][ "Siren" ][ "targetHeight" ];
			sirenConfig.tileSize					= j[ "app" ][ "Siren" ][ "tileSize" ];
			sirenConfig.tilesBetweenQueries			= j[ "app" ][ "Siren" ][ "tilesBetweenQueries" ];
			sirenConfig.tilesMSLimit				= j[ "app" ][ "Siren" ][ "tilesMSLimit" ];
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

			// setup performance monitors
			sirenConfig.timeHistory.resize( sirenConfig.performanceHistorySamples );
			sirenConfig.tileHistory.resize( sirenConfig.performanceHistorySamples );
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );
		const uint8_t * state = SDL_GetKeyboardState( NULL );

		if ( state[ SDL_SCANCODE_R ] ) {
			ResetAccumulators();
		}

		if ( state[ SDL_SCANCODE_T ] ) {
			glMemoryBarrier( GL_ALL_BARRIER_BITS );
			ScreenShots( true, true, true );
		}

		// reload shaders
		if ( state[ SDL_SCANCODE_Y ] ) {
			ReloadShaders();
		}

	}

	void ImguiPass () {
		ZoneScoped;

		{
			// controls, perf monitoring window
			ImGui::Begin( "Controls Window", NULL, 0 );

			// timing history
			const std::vector< float > timeVector = { sirenConfig.timeHistory.begin(), sirenConfig.timeHistory.end() };
			const string timeLabel = string( "Average: " ) + std::to_string( std::reduce( timeVector.begin(), timeVector.end() ) / timeVector.size() ).substr( 0, 5 ) + string( "ms/frame" );
			ImGui::PlotLines( " ", timeVector.data(), sirenConfig.performanceHistorySamples, 0, timeLabel.c_str(), -5.0f, 60.0f, ImVec2( ImGui::GetWindowSize().x - 30, 65 ) );

			// tiling history
			const std::vector< float > tileVector = { sirenConfig.tileHistory.begin(), sirenConfig.tileHistory.end() };
			const string tileLabel = string( "Average: " ) + std::to_string( std::reduce( tileVector.begin(), tileVector.end() ) / tileVector.size() ).substr( 0, 5 ) + string( " tiles/update" );
			ImGui::PlotLines( " ", tileVector.data(), sirenConfig.performanceHistorySamples, 0, tileLabel.c_str(), -10.0f, 200.0f, ImVec2( ImGui::GetWindowSize().x - 30, 65 ) );

			ImGui::End();
		}

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		QuitConf( &quitConfirm ); // show quit confirm modal window, if triggered
	}

	void ScreenShots ( const bool colorEXR = false, const bool normalEXR = false, const bool tonemappedResult = false ) {
		if ( colorEXR == true ) {
			std::vector< float > imageBytesToSave;
			imageBytesToSave.resize( sirenConfig.targetWidth * sirenConfig.targetHeight * sizeof( float ) * 4, 0 );
			glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Color Accumulator" ) );
			glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
			Image_4F screenshot( sirenConfig.targetWidth, sirenConfig.targetHeight, &imageBytesToSave.data()[ 0 ] );
			const string filename = string( "ColorAccumulator-" ) + timeDateString() + string( ".exr" );
			screenshot.Save( filename, Image_4F::backend::TINYEXR );
		}

		if ( normalEXR == true ) {
			std::vector< float > imageBytesToSave;
			imageBytesToSave.resize( sirenConfig.targetWidth * sirenConfig.targetHeight * sizeof( float ) * 4, 0 );
			glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Depth/Normals Accumulator" ) );
			glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
			Image_4F screenshot( sirenConfig.targetWidth, sirenConfig.targetHeight, &imageBytesToSave.data()[ 0 ] );
			const string filename = string( "NormalDepthAccumulator-" ) + timeDateString() + string( ".exr" );
			screenshot.Save( filename, Image_4F::backend::TINYEXR );
		}

		if ( tonemappedResult == true ) {
			std::vector< uint8_t > imageBytesToSave;
			imageBytesToSave.resize( config.width * config.height * 4, 0 );
			glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Display Texture" ) );
			glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, &imageBytesToSave.data()[ 0 ] );
			Image_4U screenshot( config.width, config.height, &imageBytesToSave.data()[ 0 ] );
			const string filename = string( "Tonemapped-" ) + timeDateString() + string( ".png" );
			screenshot.FlipVertical(); // whatever
			screenshot.Save( filename );
		}
	}

	GLuint64 SubmitTimerAndWait ( GLuint timer ) {
		glQueryCounter( timer, GL_TIMESTAMP );
		GLint available = 0;
		while ( !available )
			glGetQueryObjectiv( timer, GL_QUERY_RESULT_AVAILABLE, &available );
		GLuint64 t;
		glGetQueryObjectui64v( timer, GL_QUERY_RESULT, &t );
		return t;
	}

	void ComputePasses () {
		ZoneScoped;

		{
			scopedTimer Start( "Tiled Update" );
			const GLuint shader = shaders[ "Pathtrace" ];
			glUseProgram( shader );

			// send uniforms ( initial, shared across all tiles dispatched this frame )
			glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), sirenConfig.blueNoiseOffset.x, sirenConfig.blueNoiseOffset.y );
			glUniform1i( glGetUniformLocation( shader, "raymarchMaxSteps" ), sirenConfig.raymarchMaxSteps );
			glUniform1i( glGetUniformLocation( shader, "raymarchMaxBounces" ), sirenConfig.raymarchMaxBounces );
			glUniform1f( glGetUniformLocation( shader, "raymarchMaxDistance" ), sirenConfig.raymarchMaxDistance );
			glUniform1f( glGetUniformLocation( shader, "raymarchEpsilon" ), sirenConfig.raymarchEpsilon );
			glUniform1f( glGetUniformLocation( shader, "raymarchUnderstep" ), sirenConfig.raymarchUnderstep );
			glUniform1f( glGetUniformLocation( shader, "exposure" ), sirenConfig.exposure );
			glUniform1f( glGetUniformLocation( shader, "FoV" ), sirenConfig.renderFoV );
			// would it make more sense to invert these? I think it's kind of perceptually backwards, somehow
			glUniform3fv( glGetUniformLocation( shader, "basisX" ), 1, glm::value_ptr( trident.basisX ) );
			glUniform3fv( glGetUniformLocation( shader, "basisY" ), 1, glm::value_ptr( trident.basisY ) );
			glUniform3fv( glGetUniformLocation( shader, "basisZ" ), 1, glm::value_ptr( trident.basisZ ) );
			glUniform3fv( glGetUniformLocation( shader, "viewerPosition" ), 1, glm::value_ptr( sirenConfig.viewerPosition ) );
			// color, depth, normal targets, blue noise source data
			textureManager.BindImageForShader( "Color Accumulator", "accumulatorColor", shader, 0 );
			textureManager.BindImageForShader( "Depth/Normals Accumulator", "accumulatorNormalsAndDepth", shader, 1 );
			textureManager.BindImageForShader( "Blue Noise", "blueNoise", shader, 2 );

			// create OpenGL timery query objects - more reliable than std::chrono, at least in theory
			GLuint t[ 2 ];
			GLuint64 t0, tCheck;
			glGenQueries( 2, t );

			// submit the first timer query, to determine t0, outside the loop
			t0 = SubmitTimerAndWait( t[ 0 ] );

			// for monitoring number of completed tiles
			uint32_t tilesThisFrame = 0;

			while ( 1 ) {
				// run some N tiles out of the list
				for ( uint32_t tile = 0; tile < sirenConfig.tilesBetweenQueries; tile++ ) {
					const ivec2 tileOffset = GetTile(); // send uniforms ( unique per loop iteration )
					glUniform2i( glGetUniformLocation( shader, "tileOffset" ), tileOffset.x, tileOffset.y );
					glUniform1i( glGetUniformLocation( shader, "wangSeed" ), sirenConfig.wangSeeder() );

					// going to basically say that tilesizes are multiples of 16, to match the group size
					glDispatchCompute( sirenConfig.tileSize / 16, sirenConfig.tileSize / 16, 1 );
					tilesThisFrame++;
				}

				// submit the second timer query, to determine tCheck, inside the loop
				tCheck = SubmitTimerAndWait( t[ 1 ] );

				// evaluate how long it we've taken in the infinite loop, and break if 16.6ms is exceeded
				float loopTime = ( tCheck - t0 ) / 1e6f; // convert ns -> ms
				if ( loopTime > sirenConfig.tilesMSLimit ) {
					// update performance monitors with latest data
					UpdatePerfMonitors( loopTime, tilesThisFrame );
					break;
				}
			}
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

	void UpdateNoiseOffset () {
		static rng offsetGenerator = rng( 0, 512 );
		sirenConfig.blueNoiseOffset = ivec2( offsetGenerator(), offsetGenerator() );
	}

	void UpdatePerfMonitors ( float loopTime, float tilesThisFrame ) {
		sirenConfig.timeHistory.push_back( loopTime );
		sirenConfig.tileHistory.push_back( tilesThisFrame );
		sirenConfig.timeHistory.pop_front();
		sirenConfig.tileHistory.pop_front();
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

	void ReloadShaders () {
		shaders[ "Pathtrace" ] = computeShader( "./src/projects/PathTracing/Siren/shaders/pathtrace.cs.glsl" ).shaderHandle;
		shaders[ "Postprocess" ] = computeShader( "./src/projects/PathTracing/Siren/shaders/postprocess.cs.glsl" ).shaderHandle;
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
			// shuffle the generated array
			std::random_device rd;
			std::mt19937 rngen( rd() );
			std::shuffle( sirenConfig.tileOffsets.begin(), sirenConfig.tileOffsets.end(), rngen );
		} else { // check if the offset needs to be reset, this means a full pass has been completed
			if ( ++sirenConfig.tileOffset == sirenConfig.tileOffsets.size() ) {
				sirenConfig.tileOffset = 0;
				sirenConfig.numFullscreenPasses++;
			}
		}
		// // shuffle when listOffset is zero ( first iteration, and any subsequent resets )
		// // I like this, for some reason, but I think it would require having a barrier for cases where the reshuffle
			// would have the same tiles at the start that had been right at the end of the previous shuffled result...
		//if ( !sirenConfig.tileOffset ) {
		//	std::random_device rd;
		//	std::mt19937 rngen( rd() );
		//	std::shuffle( sirenConfig.tileOffsets.begin(), sirenConfig.tileOffsets.end(), rngen );
		//	// UpdateNoiseOffset(); // this pass's blue noise offset
		//}
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

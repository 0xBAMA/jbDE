#include "../../../engine/engine.h"

// program parameters and state
struct sirenConfig_t {
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
	bool rendererNeedsUpdate = true;	// eventually to allow for the preview modes to render once between orientation etc changes
	std::vector< ivec2 > tileOffsets;	// shuffled list of tiles
	uint32_t tileOffset = 0;			// offset into tile list - start at first element

	ivec2 blueNoiseOffset;
	float exposure;
	float renderFoV;
	rngi wangSeeder = rngi( 0, 100000000 );	// initial value for the wang ( +x,y offset per invocation )

	// viewer position, orientation
	vec3 viewerPosition;
	vec3 basisX = vec3( 1.0f, 0.0f, 0.0f );
	vec3 basisY = vec3( 0.0f, 1.0f, 0.0f );
	vec3 basisZ = vec3( 0.0f, 0.0f, 1.0f );

	// enhanced sphere tracing paper - potential for some optimizations - over-relaxation, refraction, dynamic epsilon
		// https://erleuchtet.org/~cupe/permanent/enhanced_sphere_tracing.pdf

	// raymarch parameters
	uint32_t raymarchMaxSteps;
	uint32_t raymarchMaxBounces;
	float raymarchMaxDistance;
	float raymarchEpsilon;
	float raymarchUnderstep;

	// scene parameters
	vec3 skylightColor = vec3( 0.4f );

	// questionable need:
		// dither parameters ( mode, colorspace, pattern )
			// dithering the pathtrace result, seems, questionable - tbd
		// depth fog parameters ( mode, scalar )
			// will probably add this in
			// additionally, with depth + normals stored, SSAO? might add something, but will have to evaluate
		// display mode ( preview depth, normals, colors, pathtrace )
		// thin lens parameters ( focus distance, disk offset jitter scale )

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

		// modifier keys state
		const SDL_Keymod k	= SDL_GetModState();
		const bool shift	= ( k & KMOD_SHIFT );
		// const bool alt		= ( k & KMOD_ALT );
		// const bool control	= ( k & KMOD_CTRL );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

		if ( state[ SDL_SCANCODE_R ] && shift ) {
			ResetAccumulators();
		}

		if ( state[ SDL_SCANCODE_T ] && shift ) {
			glMemoryBarrier( GL_ALL_BARRIER_BITS );
			ScreenShots( false, false, true );
		}

		// reload shaders
		if ( state[ SDL_SCANCODE_Y ] ) {
			ReloadShaders();
		}

		// quaternion based rotation via retained state in the basis vectors - much easier to use than euler angles or the trident
		const float scalar = shift ? 0.02f : 0.0005f;
		if ( state[ SDL_SCANCODE_W ] ) {
			glm::quat rot = glm::angleAxis( -scalar, sirenConfig.basisX ); // basisX is the axis, therefore remains untransformed
			sirenConfig.basisY = ( rot * vec4( sirenConfig.basisY, 0.0f ) ).xyz();
			sirenConfig.basisZ = ( rot * vec4( sirenConfig.basisZ, 0.0f ) ).xyz();
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_S ] ) {
			glm::quat rot = glm::angleAxis( scalar, sirenConfig.basisX );
			sirenConfig.basisY = ( rot * vec4( sirenConfig.basisY, 0.0f ) ).xyz();
			sirenConfig.basisZ = ( rot * vec4( sirenConfig.basisZ, 0.0f ) ).xyz();
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_A ] ) {
			glm::quat rot = glm::angleAxis( -scalar, sirenConfig.basisY ); // same as above, but basisY is the axis
			sirenConfig.basisX = ( rot * vec4( sirenConfig.basisX, 0.0f ) ).xyz();
			sirenConfig.basisZ = ( rot * vec4( sirenConfig.basisZ, 0.0f ) ).xyz();
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_D ] ) {
			glm::quat rot = glm::angleAxis( scalar, sirenConfig.basisY );
			sirenConfig.basisX = ( rot * vec4( sirenConfig.basisX, 0.0f ) ).xyz();
			sirenConfig.basisZ = ( rot * vec4( sirenConfig.basisZ, 0.0f ) ).xyz();
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_Q ] ) {
			glm::quat rot = glm::angleAxis( -scalar, sirenConfig.basisZ ); // and again for basisZ
			sirenConfig.basisX = ( rot * vec4( sirenConfig.basisX, 0.0f ) ).xyz();
			sirenConfig.basisY = ( rot * vec4( sirenConfig.basisY, 0.0f ) ).xyz();
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_E ] ) {
			glm::quat rot = glm::angleAxis( scalar, sirenConfig.basisZ );
			sirenConfig.basisX = ( rot * vec4( sirenConfig.basisX, 0.0f ) ).xyz();
			sirenConfig.basisY = ( rot * vec4( sirenConfig.basisY, 0.0f ) ).xyz();
			sirenConfig.rendererNeedsUpdate = true;
		}

		// f to reset basis, shift + f to reset basis and home to origin
		if ( state[ SDL_SCANCODE_F ] ) {
			if ( shift ) {
				sirenConfig.viewerPosition = vec3( 0.0f, 0.0f, 0.0f );
			}
			// reset to default basis
			sirenConfig.basisX = vec3( 1.0f, 0.0f, 0.0f );
			sirenConfig.basisY = vec3( 0.0f, 1.0f, 0.0f );
			sirenConfig.basisZ = vec3( 0.0f, 0.0f, 1.0f );
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_UP ] ) {
			sirenConfig.viewerPosition += scalar * sirenConfig.basisZ;
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_DOWN ] ) {
			sirenConfig.viewerPosition -= scalar * sirenConfig.basisZ;
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_RIGHT ] ) {
			sirenConfig.viewerPosition += scalar * sirenConfig.basisX;
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_LEFT ] ) {
			sirenConfig.viewerPosition -= scalar * sirenConfig.basisX;
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_PAGEUP ] ) {
			sirenConfig.viewerPosition += scalar * sirenConfig.basisY;
			sirenConfig.rendererNeedsUpdate = true;
		}
		if ( state[ SDL_SCANCODE_PAGEDOWN ] ) {
			sirenConfig.viewerPosition -= scalar * sirenConfig.basisY;
			sirenConfig.rendererNeedsUpdate = true;
		}

	}

	void ImguiPass () {
		ZoneScoped;

		{
			// controls, perf monitoring window
			ImGui::Begin( "Controls Window", NULL, 0 );

			// rendering parameters
				// view position
				// basis vectors? not sure
				// tile size
				// tiles between queries
				// tiles ms limit
				// render FoV
				// exposure

			// raymarch parameters
				// max steps
				// max bounces
				// max distance
				// epsilon
				// understep

			// scene parameters
				// skylight color

			// timing history
			const std::vector< float > timeVector = { sirenConfig.timeHistory.begin(), sirenConfig.timeHistory.end() };
			const string timeLabel = string( "Average: " ) + std::to_string( std::reduce( timeVector.begin(), timeVector.end() ) / timeVector.size() ).substr( 0, 5 ) + string( "ms/frame" );
			ImGui::PlotLines( " ", timeVector.data(), sirenConfig.performanceHistorySamples, 0, timeLabel.c_str(), -5.0f, 60.0f, ImVec2( ImGui::GetWindowSize().x - 30, 65 ) );

			// tiling history
			const std::vector< float > tileVector = { sirenConfig.tileHistory.begin(), sirenConfig.tileHistory.end() };
			const string tileLabel = string( "Average: " ) + std::to_string( std::reduce( tileVector.begin(), tileVector.end() ) / tileVector.size() ).substr( 0, 5 ) + string( " tiles/update" );
			ImGui::PlotLines( " ", tileVector.data(), sirenConfig.performanceHistorySamples, 0, tileLabel.c_str(), -10.0f, 200.0f, ImVec2( ImGui::GetWindowSize().x - 30, 65 ) );

			// report number of complete passes, average tiles per second, average effective rays per second

			ImGui::End();
		}

		if ( tonemap.showTonemapWindow ) {
			TonemapControlsWindow();
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
			glUniform3fv( glGetUniformLocation( shader, "skylightColor" ), 1, glm::value_ptr( sirenConfig.skylightColor ) );
			glUniform3fv( glGetUniformLocation( shader, "basisX" ), 1, glm::value_ptr( sirenConfig.basisX ) );
			glUniform3fv( glGetUniformLocation( shader, "basisY" ), 1, glm::value_ptr( sirenConfig.basisY ) );
			glUniform3fv( glGetUniformLocation( shader, "basisZ" ), 1, glm::value_ptr( sirenConfig.basisZ ) );
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
			// potentially add stuff like dithering, SSAO, depth fog, etc
			scopedTimer Start( "Postprocess" );
			const GLuint shader = shaders[ "Postprocess" ];
			glUseProgram( shader );

			textureManager.BindTexForShader( "Color Accumulator", "source", shader, 0 );
			textureManager.BindImageForShader( "Display Texture", "displayTexture", shader, 1 );

			// make sure we're sending the updated color temp every frame
			static float prevColorTemperature = 0.0f;
			static vec3 temperatureColor;
			if ( tonemap.colorTemp != prevColorTemperature ) {
				prevColorTemperature = tonemap.colorTemp;
				temperatureColor = GetColorForTemperature( tonemap.colorTemp );
			}

			// precompute the 3x3 matrix for the saturation adjustment
			static float prevSaturationValue = -1.0f;
			static mat3 saturationMatrix;
			if ( tonemap.saturation != prevSaturationValue ) {
				// https://www.graficaobscura.com/matrix/index.html
				const float s = tonemap.saturation;
				const float oms = 1.0f - s;

				vec3 weights = tonemap.saturationImprovedWeights ?
					vec3( 0.3086f, 0.6094f, 0.0820f ) :	// "improved" luminance vector
					vec3( 0.2990f, 0.5870f, 0.1140f );	// NTSC weights

				saturationMatrix = mat3(
					oms * weights.r + s,	oms * weights.r,		oms * weights.r,
					oms * weights.g,		oms * weights.g + s,	oms * weights.g,
					oms * weights.b,		oms * weights.b,		oms * weights.b + s
				);
			}

			glUniform3fv( glGetUniformLocation( shader, "colorTempAdjust" ), 1, glm::value_ptr( temperatureColor ) );
			glUniform1i( glGetUniformLocation( shader, "tonemapMode" ), tonemap.tonemapMode );
			glUniformMatrix3fv( glGetUniformLocation( shader, "saturation" ), 1, false, glm::value_ptr( saturationMatrix ) );
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
			for ( uint32_t x = 0; x <= sirenConfig.targetWidth; x += sirenConfig.tileSize ) {
				for ( uint32_t y = 0; y <= sirenConfig.targetHeight; y += sirenConfig.tileSize ) {
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

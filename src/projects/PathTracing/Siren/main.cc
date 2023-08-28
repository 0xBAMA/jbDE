#include "../../../engine/engine.h"

// program parameters and state
struct sirenConfig_t {
	// performance settings / monitoring
	uint32_t performanceHistorySamples;
	std::deque< float > timeHistory;	// ms per frame
	std::deque< float > tileHistory;	// completed tiles per frame
	uint32_t numFullscreenPasses = 0;
	int32_t sampleCountCap;				// -1 for unlimited
	bool showTimeStamp = false;

	// add time since last reset ( + report avg time per fullscreen pass )
	std::chrono::time_point< std::chrono::steady_clock > tLastReset = std::chrono::steady_clock::now();

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
	int subpixelJitterMethod;
	float exposure;
	float renderFoV;
	float uvScalar;
	int cameraType;

	// thin lens parameters
	bool thinLensEnable;
	float thinLensFocusDistance;
	float thinLensJitterRadius;

	// noise, rng
	ivec2 blueNoiseOffset;
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
	vec3 skylightColor = vec3( 0.1f );
	float mainLightTemperature = 6500.0f;

	// questionable need:
		// dither parameters ( mode, colorspace, pattern )
			// dithering the pathtrace result, seems, questionable - tbd
		// depth fog parameters ( mode, scalar )
			// will probably add this in
			// additionally, with depth + normals stored, SSAO? might add something, but will have to evaluate
		// display mode ( preview depth, normals, colors, pathtrace )

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

			ReloadConfig();

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

		// https://wiki.libsdl.org/SDL2/SDL_GetMouseState
			// want to pick depth at mouse location, map the position and then take data from the depth buffer, on click
				// maybe the basis for some kind of autofocus feature for the thin lens? I really like this idea

		// https://wiki.libsdl.org/SDL2/SDL_GetRelativeMouseState
			// I believe this is related to clicking and dragging

		int mouseX, mouseY;
		uint32_t mouseState = SDL_GetMouseState( &mouseX, &mouseY );
		if ( mouseState != 0 ) {
			// cout << "Mouse at " << mouseX << " " << mouseY << " with bitmask " << std::bitset<32>( mouseState ) << endl;
			vec2 fractionalPosition = vec2( float( mouseX ) / float( config.width ), 1.0f - ( float( mouseY ) / float( config.height ) ) );
			ivec2 pickPosition = ivec2( fractionalPosition.x * sirenConfig.targetWidth, fractionalPosition.y * sirenConfig.targetHeight );

			if ( pickPosition.x >= 0 && pickPosition.y >= 0 && pickPosition.x < config.width && pickPosition.y < config.height ) {
				// get the value from the depth buffer - helps determine where to set the focal plane for the DoF
				float value[ 4 ];
				const GLuint texture = textureManager.Get( "Depth/Normals Accumulator" );
				glGetTextureSubImage( texture, 0, pickPosition.x, pickPosition.y, 0, 1, 1, 1, GL_RGBA, GL_FLOAT, 16, &value );
				// cout << "Picked depth at " << config.width << " " << config.height << " is " << value[ 3 ] << endl;
				if ( shift == true ) {
					sirenConfig.thinLensFocusDistance = value[ 3 ];
				}
			}
		}

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

			// controls
			ImGui::SeparatorText( "Screenshots" );
			if ( ImGui::Button( "Linear Color EXR" ) ) {
			// EXR screenshot for linear color
				ScreenShots( true, false, false, false );
			}

			ImGui::SameLine();
			if ( ImGui::Button( "Normal/Depth EXR" ) ) {
			// EXR screenshot for normals/depth
				ScreenShots( false, true, false, false );
			}

			ImGui::SameLine();
			if ( ImGui::Button( "Tonemapped LDR PNG" ) ) {
			// PNG screenshot for tonemapped result
				ScreenShots( false, false, true, false );
			}

			ImGui::Text( " " );
			ImGui::SeparatorText( "Renderer Controls" );
			if ( ImGui::Button( "Reset Accumulators" ) ) {
				ResetAccumulators();
			}

			ImGui::SameLine();
			if ( ImGui::Button( "Reload Shaders" ) ) {
				ReloadShaders();
			}

			ImGui::SameLine();
			if ( ImGui::Button( "Reload Config" ) ) {
				ReloadConfig();
			}

			ImGui::Text( "Resolution" );
			static int x = sirenConfig.targetWidth;
			static int y = sirenConfig.targetHeight;
			ImGui::SliderInt( "Accumulator X", &x, 0, 5000 );
			ImGui::SliderInt( "Accumulator Y", &y, 0, 5000 );
			if ( ImGui::Button( "Resize" ) ) {
				ResizeAccumulators( x, y );
			}
			ImGui::SameLine();
			if ( ImGui::Button( " 360p " ) ) {
				x = 640;
				y = 360;
				ResizeAccumulators( 640, 360 );
			}
			ImGui::SameLine();
			if ( ImGui::Button( " 720p " ) ) {
				x = 1280;
				y = 720;
				ResizeAccumulators( 1280, 720 );
			}
			ImGui::SameLine();
			if ( ImGui::Button( " 1080p " ) ) {
				x = 1920;
				y = 1080;
				ResizeAccumulators( 1920, 1080 );
			}
			ImGui::SameLine();
			if ( ImGui::Button( " 4K " ) ) {
				x = 3840;
				y = 2160;
				ResizeAccumulators( 3840, 2160 );
			}

		// rendering parameters
			ImGui::Text( " " );
			ImGui::SeparatorText( "Rendering Parameters" );
			// view position - consider adding 3 component float input SliderFloat3
			ImGui::SliderFloat( "Viewer X", &sirenConfig.viewerPosition.x, -20.0f, 20.0f );
			ImGui::SliderFloat( "Viewer Y", &sirenConfig.viewerPosition.y, -20.0f, 20.0f );
			ImGui::SliderFloat( "Viewer Z", &sirenConfig.viewerPosition.z, -20.0f, 20.0f );

			// tile size
			ImGui::Text( "Tile Size: %d", sirenConfig.tileSize );
			ImGui::SameLine();
			// update will require that we rebuild the tile offset list
			if ( ImGui::Button( " - " ) ) {
				sirenConfig.tileSize = std::clamp( sirenConfig.tileSize >> 1, 16u, 4096u );
				sirenConfig.tileListNeedsUpdate = true;
			}
			ImGui::SameLine();
			if ( ImGui::Button( " + " ) ) {
				sirenConfig.tileSize = std::clamp( sirenConfig.tileSize << 1, 16u, 4096u );
				sirenConfig.tileListNeedsUpdate = true;
			}

			ImGui::SliderInt( "Tiles Between Queries", ( int * ) &sirenConfig.tilesBetweenQueries, 0, 45, "%d" );
			ImGui::SliderFloat( "Frame Time Limit (ms)", &sirenConfig.tilesMSLimit, 1.0f, 1000.0f, "%.3f", ImGuiSliderFlags_Logarithmic );
			ImGui::SliderFloat( "Render FoV", &sirenConfig.renderFoV, 0.01f, 3.0f, "%.3f", ImGuiSliderFlags_Logarithmic );
			ImGui::SliderFloat( "Screen UV Scalar", &sirenConfig.uvScalar, 0.01f, 5.0f );
			ImGui::SliderFloat( "Exposure", &sirenConfig.exposure, 0.0f, 5.0f );
			ImGui::Text( " " );

			const char * cameraNames[] = { "NORMAL", "SPHERICAL", "SPHERICAL2", "SPHEREBUG", "SIMPLEORTHO", "ORTHO" };
			ImGui::Combo( "Camera Type", &sirenConfig.cameraType, cameraNames, IM_ARRAYSIZE( cameraNames ) );

			const char * subpixelJitterModes[] = { "NONE", "BLUE", "UNIFORM", "WEYL", "WEYL INTEGER" };
			ImGui::Combo( "Subpixel Jitter", &sirenConfig.subpixelJitterMethod, subpixelJitterModes, IM_ARRAYSIZE( subpixelJitterModes ) );

			ImGui::Text( " " );
			ImGui::Checkbox( "Enable Thin Lens DoF", &sirenConfig.thinLensEnable );
			ImGui::SliderFloat( "Thin Lens Focus Distance", &sirenConfig.thinLensFocusDistance, 0.0f, 40.0f, "%.3f", ImGuiSliderFlags_Logarithmic );
			ImGui::SliderFloat( "Thin Lens Jitter Radius", &sirenConfig.thinLensJitterRadius, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic );
			ImGui::Text( " " );
			ImGui::Checkbox( "Show Timestamp", &sirenConfig.showTimeStamp );

		// raymarch parameters
			ImGui::Text( " " );
			ImGui::SeparatorText( "Raymarch Parameters" );
			ImGui::SliderInt( "Max Steps", ( int * ) &sirenConfig.raymarchMaxSteps, 10, 500 );
			ImGui::SliderInt( "Max Bounces", ( int * ) &sirenConfig.raymarchMaxBounces, 0, 50 );
			ImGui::SliderFloat( "Max Distance", &sirenConfig.raymarchMaxDistance, 1.0f, 500.0f );
			ImGui::SliderFloat( "Epsilon", &sirenConfig.raymarchEpsilon, 0.0001f, 0.5f, "%.5f", ImGuiSliderFlags_Logarithmic );
			ImGui::SliderFloat( "Understep", &sirenConfig.raymarchUnderstep, 0.1f, 1.0f );

		// scene parameters
			ImGui::Text( " " );
			ImGui::SeparatorText( "Scene Parameters" );
			// skylight color

			ImGui::Text( " " );
			ImGui::SeparatorText( "Performance" );
			float ts = std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::steady_clock::now() - sirenConfig.tLastReset ).count() / 1000.0f;
			ImGui::Text( "Fullscreen Passes: %d in %.3f seconds ( %.3f samples/sec )", sirenConfig.numFullscreenPasses, ts, ( float ) sirenConfig.numFullscreenPasses / ts );
			ImGui::SeparatorText( "Time History" );
			// timing history
			const std::vector< float > timeVector = { sirenConfig.timeHistory.begin(), sirenConfig.timeHistory.end() };
			const string timeLabel = string( "Average: " ) + std::to_string( std::reduce( timeVector.begin(), timeVector.end() ) / timeVector.size() ).substr( 0, 5 ) + string( "ms/frame" );
			ImGui::PlotLines( " ", timeVector.data(), sirenConfig.performanceHistorySamples, 0, timeLabel.c_str(), -5.0f, 180.0f, ImVec2( ImGui::GetWindowSize().x - 30, 65 ) );

			ImGui::SeparatorText( "Tile History" );
			// tiling history
			const std::vector< float > tileVector = { sirenConfig.tileHistory.begin(), sirenConfig.tileHistory.end() };
			const string tileLabel = string( "Average: " ) + std::to_string( std::reduce( tileVector.begin(), tileVector.end() ) / tileVector.size() ).substr( 0, 5 ) + string( " tiles/update" );
			ImGui::PlotLines( " ", tileVector.data(), sirenConfig.performanceHistorySamples, 0, tileLabel.c_str(), -10.0f, 2000.0f, ImVec2( ImGui::GetWindowSize().x - 30, 65 ) );

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

	void ColorScreenShotWithFilename ( const string filename ) {
		std::vector< uint8_t > imageBytesToSave;
		imageBytesToSave.resize( config.width * config.height * 4, 0 );
		glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Display Texture" ) );
		glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, &imageBytesToSave.data()[ 0 ] );
		Image_4U screenshot( config.width, config.height, &imageBytesToSave.data()[ 0 ] );
		screenshot.FlipVertical(); // whatever
		screenshot.Save( filename );
	}

	void ScreenShots (
		const bool colorEXR = false,
		const bool normalEXR = false,
		const bool tonemappedResult = false,
		const bool tonemappedFullRes = false ) {

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
			const string filename = string( "Tonemapped-" ) + timeDateString() + string( ".png" );
			ColorScreenShotWithFilename( filename );
		}

		if ( tonemappedFullRes == true ) {
			// need to have another buffer, at the corresponding resolution - tonemap into this, rather than the display texture
				// another tile-based thing here, so that it will scale with the rest of the pipeline up to the driver limits on texture resolution
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
			glUniform1f( glGetUniformLocation( shader, "uvScalar" ), sirenConfig.uvScalar );
			glUniform1i( glGetUniformLocation( shader, "cameraType" ), sirenConfig.cameraType );
			glUniform1i( glGetUniformLocation( shader, "subpixelJitterMethod" ), sirenConfig.subpixelJitterMethod );
			glUniform1i( glGetUniformLocation( shader, "frameNumber" ), sirenConfig.numFullscreenPasses );
			glUniform1i( glGetUniformLocation( shader, "thinLensEnable" ), sirenConfig.thinLensEnable );
			glUniform1f( glGetUniformLocation( shader, "thinLensFocusDistance" ), sirenConfig.thinLensFocusDistance );
			glUniform1f( glGetUniformLocation( shader, "thinLensJitterRadius" ), sirenConfig.thinLensJitterRadius );
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

			while ( 1 && !quitConfirm ) {
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
			if ( sirenConfig.showTimeStamp == true ) {
				scopedTimer Start( "Text Rendering" );
				textRenderer.Update( ImGui::GetIO().DeltaTime );
				textRenderer.Draw( textureManager.Get( "Display Texture" ) );
				glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
			}
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

		// reset number of samples + tile offset
		sirenConfig.numFullscreenPasses = 0;
		sirenConfig.tileOffset = 0;

		// reset time since last reset
		sirenConfig.tLastReset = std::chrono::steady_clock::now();
	}

	void ResizeAccumulators ( uint32_t x, uint32_t y ) {
		// destroy the existing textures
		textureManager.Remove( "Depth/Normals Accumulator" );
		textureManager.Remove( "Color Accumulator" );

		// create the new ones
		textureOptions_t opts;
		opts.dataType		= GL_RGBA32F;
		opts.width			= sirenConfig.targetWidth = x;
		opts.height			= sirenConfig.targetHeight = y;
		opts.minFilter		= GL_LINEAR;
		opts.magFilter		= GL_LINEAR;
		opts.textureType	= GL_TEXTURE_2D;
		textureManager.Add( "Depth/Normals Accumulator", opts );
		textureManager.Add( "Color Accumulator", opts );

		// rebuild tile list next frame
		sirenConfig.tileListNeedsUpdate = true;
		sirenConfig.numFullscreenPasses = 0;
	}

	void ReloadShaders () {
		shaders[ "Pathtrace" ] = computeShader( "./src/projects/PathTracing/Siren/shaders/pathtrace.cs.glsl" ).shaderHandle;
		shaders[ "Postprocess" ] = computeShader( "./src/projects/PathTracing/Siren/shaders/postprocess.cs.glsl" ).shaderHandle;
	}

	void ReloadConfig () {
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
		sirenConfig.subpixelJitterMethod		= j[ "app" ][ "Siren" ][ "subpixelJitterMethod" ];
		sirenConfig.exposure					= j[ "app" ][ "Siren" ][ "exposure" ];
		sirenConfig.renderFoV					= j[ "app" ][ "Siren" ][ "renderFoV" ];
		sirenConfig.uvScalar					= j[ "app" ][ "Siren" ][ "uvScalar" ];
		sirenConfig.cameraType					= j[ "app" ][ "Siren" ][ "cameraType" ];
		sirenConfig.thinLensEnable				= j[ "app" ][ "Siren" ][ "thinLensEnable" ];
		sirenConfig.thinLensFocusDistance		= j[ "app" ][ "Siren" ][ "thinLensFocusDistance" ];
		sirenConfig.thinLensJitterRadius		= j[ "app" ][ "Siren" ][ "thinLensJitterRadius" ];
		sirenConfig.viewerPosition.x			= j[ "app" ][ "Siren" ][ "viewerPosition" ][ "x" ];
		sirenConfig.viewerPosition.y			= j[ "app" ][ "Siren" ][ "viewerPosition" ][ "y" ];
		sirenConfig.viewerPosition.z			= j[ "app" ][ "Siren" ][ "viewerPosition" ][ "z" ];
		sirenConfig.basisX.x					= j[ "app" ][ "Siren" ][ "basisX" ][ "x" ];
		sirenConfig.basisX.y					= j[ "app" ][ "Siren" ][ "basisX" ][ "y" ];
		sirenConfig.basisX.z					= j[ "app" ][ "Siren" ][ "basisX" ][ "z" ];
		sirenConfig.basisY.x					= j[ "app" ][ "Siren" ][ "basisY" ][ "x" ];
		sirenConfig.basisY.y					= j[ "app" ][ "Siren" ][ "basisY" ][ "y" ];
		sirenConfig.basisY.z					= j[ "app" ][ "Siren" ][ "basisY" ][ "z" ];
		sirenConfig.basisZ.x					= j[ "app" ][ "Siren" ][ "basisZ" ][ "x" ];
		sirenConfig.basisZ.y					= j[ "app" ][ "Siren" ][ "basisZ" ][ "y" ];
		sirenConfig.basisZ.z					= j[ "app" ][ "Siren" ][ "basisZ" ][ "z" ];
		sirenConfig.showTimeStamp				= j[ "app" ][ "Siren" ][ "showTimeStamp" ];
	}

	void LookAt ( const vec3 eye, const vec3 at, const vec3 up ) {
		sirenConfig.viewerPosition = eye;
		sirenConfig.basisZ = normalize( at - eye );
		sirenConfig.basisX = normalize( glm::cross( up, sirenConfig.basisZ ) );
		sirenConfig.basisY = normalize( glm::cross( sirenConfig.basisZ, sirenConfig.basisX ) );
	}

	ivec2 GetTile () {
		if ( sirenConfig.tileListNeedsUpdate == true ) {
			// construct the tile list ( runs at frame 0 and again any time the tilesize changes )
			sirenConfig.tileListNeedsUpdate = false;
			sirenConfig.tileOffset = 0;
			sirenConfig.tileOffsets.resize( 0 );
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
				UpdateNoiseOffset();
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

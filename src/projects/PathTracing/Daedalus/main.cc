#include "../../../engine/engine.h"
#include "daedalusConfig.h"

class Daedalus : public engineBase {	// example derived class
public:
	Daedalus () { Init(); OnInit(); PostInit(); }
	~Daedalus () { Quit(); }

	daedalusConfig_t daedalusConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// to put the data into the accumulator
			shaders[ "Pathtrace" ] = computeShader( "./src/projects/PathTracing/Daedalus/shaders/pathtrace.cs.glsl" ).shaderHandle;
			shaders[ "Prepare" ] = computeShader( "./src/projects/PathTracing/Daedalus/shaders/prepare.cs.glsl" ).shaderHandle;
			shaders[ "Present" ] = computeShader( "./src/projects/PathTracing/Daedalus/shaders/present.cs.glsl" ).shaderHandle;

			// create the new accumulator(s)
			textureOptions_t opts;
			opts.dataType		= GL_RGBA32F;
			opts.width			= daedalusConfig.targetWidth;
			opts.height			= daedalusConfig.targetHeight;
			// opts.minFilter		= GL_LINEAR_MIPMAP_LINEAR;
			opts.minFilter		= GL_LINEAR;
			opts.magFilter		= GL_LINEAR;
			opts.textureType	= GL_TEXTURE_2D;
			opts.wrap			= GL_CLAMP_TO_BORDER;
			textureManager.Add( "Color Accumulator", opts );
			textureManager.Add( "Color Accumulator Scratch", opts );
			textureManager.Add( "Tonemapped", opts );
			textureManager.Add( "Depth/Normals Accumulator", opts );

			// setup performance monitor
			daedalusConfig.timeHistory.resize( daedalusConfig.performanceHistorySamples );


			palette::PickRandomPalette( true );

		}

	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// // current state of the whole keyboard
		const uint8_t * state = SDL_GetKeyboardState( NULL );

		if ( state[ SDL_SCANCODE_S ] ) {
			palette::PickRandomPalette( true );
		}

		// // current state of the modifier keys
		// const SDL_Keymod k	= SDL_GetModState();
		// const bool shift		= ( k & KMOD_SHIFT );
		// const bool alt		= ( k & KMOD_ALT );
		// const bool control	= ( k & KMOD_CTRL );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

		// float scrollAmount = ImGui::GetIO().MouseWheel;
		// daedalusConfig.outputZoom -= scrollAmount * 0.08f;

		ivec2 mouse;
		uint32_t mouseState = SDL_GetMouseState( &mouse.x, &mouse.y );
		if ( mouseState != 0 && !ImGui::GetIO().WantCaptureMouse ) {
			// vec2 fractionalPosition = vec2( float( mouse.x ) / float( daedalusConfig.targetWidth ), 1.0f - ( float( mouse.y ) / float( daedalusConfig.targetHeight ) ) );
			ImVec2 currentMouseDrag = ImGui::GetMouseDragDelta( 0 );
			ImGui::ResetMouseDragDelta();
			const float aspectRatio = ( float ) daedalusConfig.targetHeight / ( float ) daedalusConfig.targetWidth;
			daedalusConfig.outputOffset.x -= currentMouseDrag.x * aspectRatio * daedalusConfig.outputZoom;
			daedalusConfig.outputOffset.y += currentMouseDrag.y * daedalusConfig.outputZoom;
		}

		SDL_Event event;
		SDL_PumpEvents();
		while ( SDL_PollEvent( &event ) ) {
			ImGui_ImplSDL2_ProcessEvent( &event ); // imgui event handling
			pQuit = config.oneShot || // swap out the multiple if statements for a big chained boolean setting the value of pQuit
				( event.type == SDL_QUIT ) ||
				( event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID( window.window ) ) ||
				( event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE && SDL_GetModState() & KMOD_SHIFT );
			if ( ( event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE ) || ( event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_X1 ) ) {
				quitConfirm = !quitConfirm; // this has to stay because it doesn't seem like ImGui::IsKeyReleased is stable enough to use
			}

			if ( event.type == SDL_MOUSEWHEEL ) {
				// float wheel_x = -event.wheel.preciseX;
				const float wheel_y = event.wheel.preciseY;

				const float previousZoom = daedalusConfig.outputZoom;
				const vec2 previousOffset = daedalusConfig.outputOffset;

				// would also be nice if this could have a little bit of smoothness added to it, inertia, whatever
				daedalusConfig.outputZoom -= wheel_y * ( ( SDL_GetModState() & KMOD_SHIFT ) ? 0.07f : 0.01f );
				daedalusConfig.outputZoom = std::clamp( daedalusConfig.outputZoom, 0.005f, 5.0f );

				// closer, but still not correct
				const vec2 previousPixelLocation = ( previousOffset + vec2( mouse ) ) * previousZoom;
				daedalusConfig.outputOffset = ( previousPixelLocation - vec2( mouse ) * daedalusConfig.outputZoom ) / daedalusConfig.outputZoom;

				// no good
				// const vec2 targetSizeHalf = vec2( config.width, config.height ) / 2.0f;
				// const vec2 previousPixelLocation = ( previousOffset + targetSizeHalf ) * previousZoom;
				// daedalusConfig.outputOffset = ( previousPixelLocation - targetSizeHalf * daedalusConfig.outputZoom ) / daedalusConfig.outputZoom;
			}
		}

	}

	void UpdatePerfMonitor ( float loopTime ) {
		ZoneScoped;
		daedalusConfig.timeHistory.push_back( loopTime );
		daedalusConfig.timeHistory.pop_front();
	}

	// move this to a header, I think
	void ShowDaedalusConfigWindow() {
		ImGui::Begin( "Viewer Window 2 ( PROTOTYPE )", NULL, ImGuiWindowFlags_NoScrollWithMouse );
		// perf stuff
		// tabs
			// rendering
			// scene
			// postprocess
			// ...

		ImGui::SeparatorText( " Performance " );
		float ts = std::chrono::duration_cast< std::chrono::milliseconds >( std::chrono::steady_clock::now() - daedalusConfig.tiles.tLastReset ).count() / 1000.0f;
		ImGui::Text( "Fullscreen Passes: %d in %.3f seconds ( %.3f samples/sec )", daedalusConfig.tiles.SampleCount(), ts, ( float ) daedalusConfig.tiles.SampleCount() / ts );
		ImGui::SeparatorText( " History " );
		// timing history
		const std::vector< float > timeVector = { daedalusConfig.timeHistory.begin(), daedalusConfig.timeHistory.end() };
		const string timeLabel = string( "Average: " ) + std::to_string( std::reduce( timeVector.begin(), timeVector.end() ) / timeVector.size() ).substr( 0, 5 ) + string( "ms/frame" );
		ImGui::PlotLines( " ", timeVector.data(), daedalusConfig.performanceHistorySamples, 0, timeLabel.c_str(), -5.0f, 180.0f, ImVec2( ImGui::GetWindowSize().x - 30, 65 ) );

		// placeholder state dump
		ImGui::SeparatorText( "Daedalus State:" );
		ImGui::Text( "  Accumulator Size: %d x %d", daedalusConfig.targetWidth, daedalusConfig.targetHeight );
		ImGui::Text( "  Tile Dispenser: %d tiles, %d x %d", daedalusConfig.tiles.Count(), daedalusConfig.tiles.tileSize, daedalusConfig.tiles.tileSize );
		ImGui::Text( "  Output: %f zoom, [%f, %f] offset", daedalusConfig.outputZoom, daedalusConfig.outputOffset.x, daedalusConfig.outputOffset.y );
		ImGui::SameLine();
		if ( ImGui::Button( "Set Zero" ) ) {
			daedalusConfig.outputOffset = ivec2( 0 );
		}

		if ( ImGui::CollapsingHeader( "Images" ) ) {

			float availableWidth = ImGui::GetContentRegionAvail().x - 20;
			float proportionalHeight = availableWidth * ( float ) config.height / ( float ) config.width;

			ImGui::SeparatorText( "Display Texture" );
			ImGui::Image( ( void* ) ( intptr_t ) textureManager.Get( "Display Texture" ), ImVec2( availableWidth, proportionalHeight ) );

			proportionalHeight = availableWidth * ( float ) daedalusConfig.targetHeight / ( float ) daedalusConfig.targetWidth;

			ImGui::SeparatorText( "Color Accumulator" );
			ImGui::Image( ( void* ) ( intptr_t ) textureManager.Get( "Color Accumulator" ), ImVec2( availableWidth, proportionalHeight ) );

			ImGui::SeparatorText( "Color Accumulator Scratch" );
			ImGui::Image( ( void* ) ( intptr_t ) textureManager.Get( "Color Accumulator Scratch" ), ImVec2( availableWidth, proportionalHeight ) );

			ImGui::SeparatorText( "Depth/Normals Accumulator" );
			ImGui::Image( ( void* ) ( intptr_t ) textureManager.Get( "Depth/Normals Accumulator" ), ImVec2( availableWidth, proportionalHeight ) );

			ImGui::SeparatorText( "Tonemapped" );
			ImGui::Image( ( void* ) ( intptr_t ) textureManager.Get( "Tonemapped" ), ImVec2( availableWidth, proportionalHeight ) );
		}

		if ( ImGui::CollapsingHeader( "Postprocess" ) ) {
			PostProcessImguiMenu();
		}

		ImGui::End();
	}

	void ImguiPass () {
		ZoneScoped;

		if ( daedalusConfig.showConfigWindow == true ) {
			ShowDaedalusConfigWindow();
		}

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	GLuint64 SubmitTimerAndWait( GLuint timer ) {
		ZoneScoped;
		glQueryCounter( timer, GL_TIMESTAMP );
		GLint available = 0;
		while ( !available ) { glGetQueryObjectiv( timer, GL_QUERY_RESULT_AVAILABLE, &available ); }
		GLuint64 t;
		glGetQueryObjectui64v( timer, GL_QUERY_RESULT, &t );
		return t;
	}

	void ComputePasses () {
		ZoneScoped;


		// do some tiles
		{
			scopedTimer Start( "Tiled Update" );

			const GLuint shader = shaders[ "Pathtrace" ];
			glUseProgram( shader );

			// AnimationUpdate();
			// SendBasePathtraceUniforms();

			textureManager.BindTexForShader( "Blue Noise", "blueNoise", shader, 0 );
			textureManager.BindTexForShader( "Color Accumulator", "accumulatorColor", shader, 1 );
			textureManager.BindTexForShader( "Depth/Normals Accumulator", "accumulatorNormalsAndDepth", shader, 2 );

			static uint32_t sampleCount = 0;
			static ivec2 blueNoiseOffset;
			if ( sampleCount != daedalusConfig.tiles.SampleCount() ) sampleCount = daedalusConfig.tiles.SampleCount(),
				blueNoiseOffset = ivec2( daedalusConfig.rng.blueNoiseOffset(), daedalusConfig.rng.blueNoiseOffset() );
			glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), blueNoiseOffset.x, blueNoiseOffset.y );

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
				const uint32_t tileSize = daedalusConfig.tiles.tileSize;
				for ( uint32_t tile = 0; tile < daedalusConfig.tiles.tilesBetweenQueries; tile++ ) {
					const ivec2 tileOffset = daedalusConfig.tiles.GetTile(); // send uniforms ( unique per loop iteration )
					glUniform2i( glGetUniformLocation( shader, "tileOffset" ),	tileOffset.x, tileOffset.y );
					glUniform1i( glGetUniformLocation( shader, "wangSeed" ),	daedalusConfig.rng.wangSeeder() );

					// going to basically say that tilesizes are multiples of 16, to match the group size
					glDispatchCompute( tileSize / 16, tileSize / 16, 1 );
					tilesThisFrame++;
				}

				// submit the second timer query, to determine tCheck, inside the loop
				tCheck = SubmitTimerAndWait( t[ 1 ] );

				// evaluate how long it we've taken in the infinite loop, and break if 16.6ms is exceeded
				float loopTime = ( tCheck - t0 ) / 1e6f; // convert ns -> ms
				if ( loopTime > daedalusConfig.tiles.tileTimeLimitMS ) {
					// update performance monitors with latest data
					UpdatePerfMonitor( loopTime );
					break;
				}
			}
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		// {
		// 	scopedTimer Start( "Prepare" );
		// 	const GLuint shader = shaders[ "Prepare" ];
		// 	glUseProgram( shader );

		// 	textureManager.BindTexForShader( "Blue Noise", "blueNoise", shader, 0 );
		// 	textureManager.BindTexForShader( "Color Accumulator", "accumulatorColor", shader, 1 );
		// 	textureManager.BindTexForShader( "Depth/Normals Accumulator", "accumulatorNormalsAndDepth", shader, 2 );
		// 	textureManager.BindTexForShader( "Tonemapped", "tonemappedResult", shader, 3 );

		// 	glDispatchCompute( ( daedalusConfig.targetWidth + 15 ) / 16, ( daedalusConfig.targetHeight + 15 ) / 16, 1 );
		// 	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		// }

		{
			scopedTimer Start( "Drawing" );
			const GLuint shader = shaders[ "Present" ];
			glUseProgram( shader );
			textureManager.BindTexForShader( "Blue Noise", "blueNoise", shader, 0 );
			textureManager.BindTexForShader( "Tonemapped", "preppedImage", shader, 1 );
			// textureManager.BindTexForShader( "Color Accumulator", "preppedImage", shader, 1 );
			textureManager.BindTexForShader( "Display Texture", "outputImage", shader, 2 );
			glUniform1f( glGetUniformLocation( shader, "scale" ), daedalusConfig.outputZoom );
			glUniform2f( glGetUniformLocation( shader, "offset" ), daedalusConfig.outputOffset.x, daedalusConfig.outputOffset.y );
			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // postprocessing - shader for color grading ( color temp, contrast, gamma ... ) + tonemapping
			scopedTimer Start( "Postprocess" );
			const GLuint shader = shaders[ "Tonemap" ];
			glUseProgram( shader );

			SendTonemappingParameters();
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );
			textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.DrawBlackBackedColorString( 1, string( "   Daedalus               " ), vec3( 1.0f, 0.5f, 0.1f ) );
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		// application-specific update code

		daedalusConfig.outputOffset.x = std::clamp( daedalusConfig.outputOffset.x, -daedalusConfig.dragBufferAmount, daedalusConfig.targetWidth + daedalusConfig.dragBufferAmount );
		daedalusConfig.outputOffset.y = std::clamp( daedalusConfig.outputOffset.y, -daedalusConfig.dragBufferAmount, daedalusConfig.targetHeight + daedalusConfig.dragBufferAmount );
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

		// derived-class-specific functionality
		OnUpdate();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	Daedalus engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

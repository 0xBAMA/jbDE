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
			CompileShaders();

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

		if ( state[ SDL_SCANCODE_Y ] && shift ) {
			CompileShaders();
		}

		// float scrollAmount = ImGui::GetIO().MouseWheel;
		// daedalusConfig.view.outputZoom -= scrollAmount * 0.08f;

		ivec2 mouse;
		uint32_t mouseState = SDL_GetMouseState( &mouse.x, &mouse.y );
		if ( mouseState != 0 && !ImGui::GetIO().WantCaptureMouse ) {
			// vec2 fractionalPosition = vec2( float( mouse.x ) / float( daedalusConfig.targetWidth ), 1.0f - ( float( mouse.y ) / float( daedalusConfig.targetHeight ) ) );
			ImVec2 currentMouseDrag = ImGui::GetMouseDragDelta( 0 );
			ImGui::ResetMouseDragDelta();
			const float aspectRatio = ( float ) daedalusConfig.targetHeight / ( float ) daedalusConfig.targetWidth;
			daedalusConfig.view.outputOffset.x -= currentMouseDrag.x * aspectRatio * daedalusConfig.view.outputZoom;
			daedalusConfig.view.outputOffset.y += currentMouseDrag.y * daedalusConfig.view.outputZoom;
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

				const float previousZoom = daedalusConfig.view.outputZoom;
				const vec2 previousOffset = daedalusConfig.view.outputOffset;

				// would also be nice if this could have a little bit of smoothness added to it, inertia, whatever
				daedalusConfig.view.outputZoom -= wheel_y * ( ( SDL_GetModState() & KMOD_SHIFT ) ? 0.07f : 0.01f );
				daedalusConfig.view.outputZoom = std::clamp( daedalusConfig.view.outputZoom, 0.005f, 5.0f );

				// closer, but still not correct
				const vec2 previousPixelLocation = ( previousOffset + vec2( mouse ) ) * previousZoom;
				daedalusConfig.view.outputOffset = ( previousPixelLocation - vec2( mouse ) * daedalusConfig.view.outputZoom ) / daedalusConfig.view.outputZoom;
			}
		}

	}

	void UpdatePerfMonitor ( float loopTime, uint32_t tileCount ) {
		ZoneScoped;
		daedalusConfig.timeHistory.push_back( loopTime );
		daedalusConfig.timeHistory.pop_front();
	}

	void ResetAccumulators();
	void ResizeAccumulators( uint32_t x, uint32_t y );

	void ShowDaedalusConfigWindow();
	void ImguiPass () {
		ZoneScoped;
		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
		if ( daedalusConfig.showConfigWindow == true ) { ShowDaedalusConfigWindow(); }
		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void SendBasePathtraceUniforms();
	void SendInnerLoopPathtraceUniforms();
	void SendPrepareUniforms();
	void SendPresentUniforms();

	GLuint64 SubmitTimerAndWait( GLuint timer ) {
		ZoneScoped;
		glQueryCounter( timer, GL_TIMESTAMP );
		GLint available = 0;
		while ( !available ) { glGetQueryObjectiv( timer, GL_QUERY_RESULT_AVAILABLE, &available ); }
		GLuint64 t;
		glGetQueryObjectui64v( timer, GL_QUERY_RESULT, &t );
		return t;
	}

	void CompileShaders() {
		shaders[ "Pathtrace" ] = computeShader( "./src/projects/PathTracing/Daedalus/shaders/pathtrace.cs.glsl" ).shaderHandle;
		shaders[ "Prepare" ] = computeShader( "./src/projects/PathTracing/Daedalus/shaders/prepare.cs.glsl" ).shaderHandle;
		shaders[ "Present" ] = computeShader( "./src/projects/PathTracing/Daedalus/shaders/present.cs.glsl" ).shaderHandle;
	}

	void ComputePasses () {
		ZoneScoped;

		{ // do some tiles, update the buffer
			scopedTimer Start( "Tiled Update" );

			const GLuint shader = shaders[ "Pathtrace" ];
			glUseProgram( shader );

			// create OpenGL timery query objects - more reliable than std::chrono, at least in theory
			GLuint t[ 2 ];
			GLuint64 tStart;
			glGenQueries( 2, t );

			// submit the first timer query, to determine tStart, outside the loop
			tStart = SubmitTimerAndWait( t[ 0 ] );

			// for monitoring number of completed tiles
			uint32_t tilesThisFrame = 0;
			SendBasePathtraceUniforms();
			const uint32_t tileSize = daedalusConfig.tiles.tileSize;
			while ( 1 && !quitConfirm ) {
				for ( uint32_t tile = 0; tile < daedalusConfig.tiles.tilesBetweenQueries; tile++ ) {
					SendInnerLoopPathtraceUniforms();
					glDispatchCompute( tileSize / 16, tileSize / 16, 1 );
					tilesThisFrame++;
				}
				float loopTime = ( SubmitTimerAndWait( t[ 1 ] ) - tStart ) / 1e6f; // convert ns -> ms
				if ( loopTime > daedalusConfig.tiles.tileTimeLimitMS ) {
					UpdatePerfMonitor( loopTime, tilesThisFrame );
					break;
				}
			}
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{	// this is the tonemapping stage, on the result ( accumulator(s) -> tonemapped )
			scopedTimer Start( "Prepare" );
			const GLuint shader = shaders[ "Prepare" ];
			glUseProgram( shader );
			SendPrepareUniforms();

			glDispatchCompute( ( daedalusConfig.targetWidth + 15 ) / 16, ( daedalusConfig.targetHeight + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{	// this is the matting, with guides
			scopedTimer Start( "Drawing" );
			const GLuint shader = shaders[ "Present" ];
			glUseProgram( shader );
			SendPresentUniforms();
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // text rendering - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );

			// toggle-able controls list, sounds like a nice to have
			textRenderer.DrawBlackBackedColorString( 2, string( " Daedalus  (?) show controls" ), vec3( 1.0f, 0.5f, 0.3f ) );

			float seconds = daedalusConfig.tiles.SecondsSinceLastReset();
			float wholeSeconds = std::floor( seconds );
			stringstream ss;
			ss << fixedWidthNumberString( daedalusConfig.tiles.SampleCount(), 6, ' ' ) << " samples in ";
			ss << fixedWidthNumberString( wholeSeconds, 5, ' ' ) << "." << fixedWidthNumberString( ( seconds - wholeSeconds ) * 1000, 3, '0' ) << "s";
			textRenderer.DrawBlackBackedColorString( 1, ss.str(), vec3( 1.0f ) );
			ss.str( "" );
			float tDelta = ImGui::GetIO().DeltaTime * 1000.0f;
			ss << " frame total:   " << std::setw( 10 ) << std::setfill( ' ' ) << std::setprecision( 4 ) << std::fixed << tDelta << "ms";
			textRenderer.DrawBlackBackedColorString( 0, ss.str(), vec3( 1.0f ) );
			
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
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

		HandleCustomEvents();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};
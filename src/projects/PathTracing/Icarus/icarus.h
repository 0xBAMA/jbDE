#include "icarusData.h"

// Icarus
	// goal is to keep this super, super high level (<250 lines, kind of idea)

class Icarus final : public engineBase {
public:
	Icarus () { Init(); OnInit(); PostInit(); }
	~Icarus () { Quit(); }

	// viewerState_t viewerState;

	vec2 offset = vec2( 0.0f );
	float scale = 1.0f;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			CompileShaders( shaders );
			AllocateTextures( textureManager );
			AllocateBuffers();

			// viewerState.width = ;
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		static bool isDragging = false;
		static vec2 offsetPush = vec2( 0.0f );
		if ( inputHandler.dragging && !isDragging ) {
			isDragging = true;
			offsetPush = offset;
		} else if ( inputHandler.dragging ) { // while this dragging is happening...
			offset = offsetPush - ( scale * vec2( inputHandler.mouseDragDelta() ) * vec2( 2.0f / std::min( config.width, config.height ) ) );
		} else {
			isDragging = false;
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

			if ( event.type == SDL_MOUSEWHEEL && !ImGui::GetIO().WantCaptureMouse ) {
				const float wheel_y = event.wheel.y;
				scale = std::clamp( scale - wheel_y * ( ( SDL_GetModState() & KMOD_SHIFT ) ? 0.07f : 0.01f ), 0.005f, 5.0f );
			}
		}

	}

	void ImguiPass () {
		ZoneScoped;
		if ( tonemap.showTonemapWindow ) {
			TonemapControlsWindow();
		}

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

		// if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			// use this for some time varying seeding type thing
			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );

			glUniform2f( glGetUniformLocation( shader, "offset" ), offset.x, offset.y );
			glUniform1f( glGetUniformLocation( shader, "scale" ), scale );

			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // postprocessing - shader for color grading ( color temp, contrast, gamma ... ) + tonemapping
			scopedTimer Start( "Postprocess" );
			bindSets[ "Postprocessing" ].apply();
			glUseProgram( shaders[ "Tonemap" ] );
			SendTonemappingParameters();
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );
			textRenderer.Clear();
			textRenderer.Update( ImGui::GetIO().DeltaTime );

			// show terminal, if active - check happens inside
			textRenderer.drawTerminal( terminal );

			// put the result on the display
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		// application-specific update code
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

		// get new data into the input handler
		inputHandler.update();

		// pass any signals into the terminal
		terminal.update( inputHandler );

		// event handling
		HandleCustomEvents();
		HandleQuitEvents();

		// derived-class-specific functionality
		OnUpdate();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};
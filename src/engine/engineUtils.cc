#include "engine.h"

void engineBase::ClearColorAndDepth () {
	ZoneScoped; scopedTimer( "Clear Color and Depth" );

	// clear the screen
	glClearColor( config.clearColor.x, config.clearColor.y, config.clearColor.z, config.clearColor.w );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	// ImGuiIO &io = ImGui::GetIO();
	// const int width = ( int ) io.DisplaySize.x;
	// const int height = ( int ) io.DisplaySize.y;
	// // prevent -1, -1 being passed on first frame, since ImGui hasn't rendered yet
	// glViewport( 0, 0, width > 0 ? width : config.width, height > 0 ? height : config.height ); // should this be elsewhere?

	glViewport( 0, 0, config.width, config.height );
}

void engineBase::SendTonemappingParameters () {
	ZoneScoped;

	static float prevColorTemperature = 0.0f;
	static vec3 temperatureColor;
	if ( tonemap.colorTemp != prevColorTemperature ) {
		prevColorTemperature = tonemap.colorTemp;
		temperatureColor = GetColorForTemperature( tonemap.colorTemp );
	}

	const GLuint shader = shaders[ "Tonemap" ];
	glUniform3fv( glGetUniformLocation( shader, "colorTempAdjust" ), 1, glm::value_ptr( temperatureColor ) );
	glUniform1i( glGetUniformLocation( shader, "tonemapMode" ), tonemap.tonemapMode );
	glUniform1f( glGetUniformLocation( shader, "gamma" ), tonemap.gamma );
}

void engineBase::BlitToScreen () {
	ZoneScoped; scopedTimer Start( "Blit to Screen" );
	const GLuint shader = shaders[ "Display" ];
	glUseProgram( shader );
	glBindVertexArray( displayVAO );

	// so this is the procedure:
	// glBindTextureUnit( 0, textures[ "Display Texture" ] ); // requires OpenGL 4.5, shouldn't be an issue - but in the wrapper, will support both modes
	// glUniform1i( glGetUniformLocation( shader, "current" ), 0 );

	textureManager.BindTexForShader( "Display Texture", "current", shaders[ "Display" ], 0 );
	glUniform2f( glGetUniformLocation( shader, "resolution" ), config.width, config.height );
	glDrawArrays( GL_TRIANGLES, 0, 3 );
}

void engineBase::HandleTridentEvents () {
	ZoneScoped;

	if ( !ImGui::GetIO().WantCaptureKeyboard ) {
		constexpr float bigStep = 0.120f;
		constexpr float lilStep = 0.008f;

		// is shift being pressed
		const bool shift = SDL_GetModState() & KMOD_SHIFT;

		// can handle multiple simultaneous inputs like this
		const uint8_t * state = SDL_GetKeyboardState( NULL );

		// update block orientation
		if ( state[ SDL_SCANCODE_W ] || state[ SDL_SCANCODE_UP ] ) {	trident.RotateX( shift ?  bigStep :  lilStep ); }
		if ( state[ SDL_SCANCODE_S ] || state[ SDL_SCANCODE_DOWN ] ) {	trident.RotateX( shift ? -bigStep : -lilStep ); }
		if ( state[ SDL_SCANCODE_A ] || state[ SDL_SCANCODE_LEFT ] ) {	trident.RotateY( shift ?  bigStep :  lilStep ); }
		if ( state[ SDL_SCANCODE_D ] || state[ SDL_SCANCODE_RIGHT ] ) {	trident.RotateY( shift ? -bigStep : -lilStep ); }
		if ( state[ SDL_SCANCODE_PAGEUP ] ) {							trident.RotateZ( shift ? -bigStep : -lilStep ); }
		if ( state[ SDL_SCANCODE_PAGEDOWN ] ) {							trident.RotateZ( shift ?  bigStep :  lilStep ); }

		// snap to cardinal directions
		if ( state[ SDL_SCANCODE_1 ] ) { trident.SetViewFront();}
		if ( state[ SDL_SCANCODE_2 ] ) { trident.SetViewRight();}
		if ( state[ SDL_SCANCODE_3 ] ) { trident.SetViewBack();	}
		if ( state[ SDL_SCANCODE_4 ] ) { trident.SetViewLeft();	}
		if ( state[ SDL_SCANCODE_5 ] ) { trident.SetViewUp();	}
		if ( state[ SDL_SCANCODE_6 ] ) { trident.SetViewDown();	}

		// trident will also contain an offset, I think this is the cleanest way to contain all this functionality
	}

	if ( !ImGui::GetIO().WantCaptureMouse ) {
		// first arg is button index - this is left click, need to also handle right click to update the offset
		ImVec2 valueRaw = ImGui::GetMouseDragDelta( 1, 0.0f );
		if ( ( valueRaw.x != 0.0f || valueRaw.y != 0.0f ) ) {
			trident.RotateY( -valueRaw.x * 0.03f );
			trident.RotateX( -valueRaw.y * 0.03f );
			ImGui::ResetMouseDragDelta( 1 );
		}
	}
}

void engineBase::HandleQuitEvents () {
	ZoneScoped;
	//==============================================================================
	// Need to keep this for pQuit handling ( force quit )
	// In particular - checking for window close and the SDL_QUIT event can't really be determined
	//  via the keyboard state, and then imgui needs it too, so can't completely kill the event
	//  polling loop - maybe eventually I'll find a more complete solution for this
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
	}
}

// for next frame's LegitProfiler data
void engineBase::PrepareProfilingData () {
	timerQueries_engine.gather();

	// get rid of last frame's prepared task data
	tasks_GPU.clear();
	tasks_CPU.clear();

	int color = 0;
	float offset_CPU = 0;
	float offset_GPU = 0;
	for ( unsigned int i = 0; i < timerQueries_engine.queries_CPU.size(); i++ ) {
		color++;
		color = color % legit::Colors::colorList.size();
		legit::ProfilerTask pt_CPU;
		legit::ProfilerTask pt_GPU;

		// calculate start and end times
		pt_CPU.startTime = offset_CPU / 1000.0f;
		offset_CPU = offset_CPU + timerQueries_engine.queries_CPU[ i ].result;
		pt_CPU.endTime = offset_CPU / 1000.0f;
		pt_CPU.name = timerQueries_engine.queries_CPU[ i ].label;
		pt_CPU.color = legit::Colors::colorList[ color ]; // do better
		tasks_CPU.push_back( pt_CPU );

		pt_GPU.startTime = offset_GPU / 1000.0f;
		offset_GPU = offset_GPU + timerQueries_engine.queries_GPU[ i ].result;
		pt_GPU.endTime = offset_GPU / 1000.0f;
		pt_GPU.name = timerQueries_engine.queries_GPU[ i ].label;
		pt_GPU.color = legit::Colors::colorList[ color ]; // do better
		tasks_GPU.push_back( pt_GPU );
	}
	timerQueries_engine.clear(); // prepare for next frame's data
}

//=============================================================================
//==== std::chrono Wrapper - Simplified Tick() / Tock() Interface =============
//=============================================================================

// no nesting, but makes for a very simple interface
	// could probably do something stack based, have Tick() push and Tock() pop
#define NOW std::chrono::steady_clock::now()
#define TIMECAST(x) std::chrono::duration_cast<std::chrono::microseconds>(x).count()/1000.0f

void engineBase::Tick () {
	tCurr = NOW;
}

float engineBase::Tock () {
	return TIMECAST( NOW - tCurr );
}

float engineBase::TotalTime () {
	return TIMECAST( NOW - tStart );
}

#undef NOW
#undef TIMECAST
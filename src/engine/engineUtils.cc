#include "engine.h"

extern timerManager timerQueries;

void engine::DrawAPIGeometry () {
	ZoneScoped; scopedTimer Start( "API Geometry" );
	// draw some shit
}

void engine::ComputePasses () {
	ZoneScoped;

	{ // dummy draw - draw something into accumulatorTexture
		scopedTimer Start( "Drawing" );
		bindSets[ "Drawing" ].apply();
		glUseProgram( shaders[ "Dummy Draw" ] );
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

	// shader to apply dithering
		// ...

	// other postprocessing
		// ...

	{ // text rendering timestamp - required texture binds are handled internally
		scopedTimer Start( "Text Rendering" );
		textRenderer.Update( ImGui::GetIO().DeltaTime );
		textRenderer.Draw( textures[ "Display Texture" ] );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}

	{ // show trident with current orientation
		scopedTimer Start( "Trident" );
		trident.Update( textures[ "Display Texture" ] );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}
}

void engine::ClearColorAndDepth () {
	ZoneScoped; scopedTimer( "Clear Color and Depth" );

	// clear the screen
	glClearColor( config.clearColor.x, config.clearColor.y, config.clearColor.z, config.clearColor.w );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	ImGuiIO &io = ImGui::GetIO();
	const int width = ( int ) io.DisplaySize.x;
	const int height = ( int ) io.DisplaySize.y;
	// prevent -1, -1 being passed on first frame, since ImGui hasn't rendered yet
	glViewport( 0, 0, width > 0 ? width : config.width, height > 0 ? height : config.height ); // should this be elsewhere?
}

void engine::SendTonemappingParameters () {
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

void engine::BlitToScreen () {
	ZoneScoped; scopedTimer Start( "Blit to Screen" );

	// display current state of the display texture - there are more efficient ways to do this, look into it
	bindSets[ "Display" ].apply();
	const GLuint shader = shaders[ "Display" ];
	glUseProgram( shader );
	glBindVertexArray( displayVAO );
	ImGuiIO &io = ImGui::GetIO();
	glUniform2f( glGetUniformLocation( shader, "resolution" ), io.DisplaySize.x, io.DisplaySize.y );
	glDrawArrays( GL_TRIANGLES, 0, 3 );
}

void engine::ImguiPass () {
	ZoneScoped; scopedTimer Start( "ImGUI Pass" );

	ImguiFrameStart();							// start the imgui frame
	TonemapControlsWindow();

	// add new profiling data and render
	static ImGuiUtils::ProfilersWindow profilerWindow;
	profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
	profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
	profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom

	ImGui::ShowDemoWindow( &showDemoWindow );	// show the demo window
	QuitConf( &quitConfirm );					// show quit confirm window, if triggered
	ImguiFrameEnd();							// finish imgui frame and put it in the framebuffer
}

void engine::HandleTridentEvents () {
	ZoneScoped; scopedTimer Start( "HandleTridentEvents" );

	if ( !ImGui::GetIO().WantCaptureKeyboard ) {
		constexpr float bigStep = 0.120f;
		constexpr float lilStep = 0.008f;

		// is shift being pressed
		const bool shift = SDL_GetModState() & KMOD_SHIFT;

		// can handle multiple simultaneous inputs like this
		const uint8_t *state = SDL_GetKeyboardState( NULL );

		// update block orientation
		if ( state[ SDL_SCANCODE_A ] || state[ SDL_SCANCODE_LEFT ] ) {	trident.RotateY( shift ? bigStep : lilStep );	}
		if ( state[ SDL_SCANCODE_D ] || state[ SDL_SCANCODE_RIGHT ] ) {	trident.RotateY( shift ? -bigStep : -lilStep );	}
		if ( state[ SDL_SCANCODE_W ] || state[ SDL_SCANCODE_UP ] ) {	trident.RotateX( shift ? bigStep : lilStep );	}
		if ( state[ SDL_SCANCODE_S ] || state[ SDL_SCANCODE_DOWN ] ) {	trident.RotateX( shift ? -bigStep : -lilStep );	}
		if ( state[ SDL_SCANCODE_PAGEUP ] ) {							trident.RotateZ( shift ? -bigStep : -lilStep );	}
		if ( state[ SDL_SCANCODE_PAGEDOWN ] ) {							trident.RotateZ( shift ? bigStep : lilStep );	}

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

void engine::HandleQuitEvents () {
	ZoneScoped; scopedTimer Start( "HandleQuitEvents" );
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
void engine::PrepareProfilingData () {
	timerQueries.gather();

	// get rid of last frame's prepared task data
	tasks_GPU.clear();
	tasks_CPU.clear();

	int color = 0;
	float offset_CPU = 0;
	float offset_GPU = 0;
	for ( unsigned int i = 0; i < timerQueries.queries_CPU.size(); i++ ) {
		color++;
		color = color % legit::Colors::colorList.size();
		legit::ProfilerTask pt_CPU;
		legit::ProfilerTask pt_GPU;
		
		// calculate start and end times
		pt_CPU.startTime = offset_CPU / 1000.0f;
		offset_CPU = offset_CPU + timerQueries.queries_CPU[ i ].result;
		pt_CPU.endTime = offset_CPU / 1000.0f;
		pt_CPU.name = timerQueries.queries_CPU[ i ].label;
		pt_CPU.color = legit::Colors::colorList[ color ];
		tasks_CPU.push_back( pt_CPU );

		pt_GPU.startTime = offset_GPU / 1000.0f;
		offset_GPU = offset_GPU + timerQueries.queries_GPU[ i ].result;
		pt_GPU.endTime = offset_GPU / 1000.0f;
		pt_GPU.name = timerQueries.queries_GPU[ i ].label;
		pt_GPU.color = legit::Colors::colorList[ color ];
		tasks_GPU.push_back( pt_GPU );
	}

	timerQueries.clear(); // prepare for next frame's data
}
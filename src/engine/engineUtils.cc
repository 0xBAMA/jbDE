#include "engine.h"

bool engine::MainLoop () {
	ZoneScoped;
	
	HandleEvents();					// handle keyboard / mouse events
	ClearColorAndDepth();			// if I just disable depth testing, this can disappear
	DrawAPIGeometry();				// draw any API geometry desired
	ComputePasses();				// multistage update of displayTexture
	BlitToScreen();					// fullscreen triangle copying the displayTexture to the screen
	ImguiPass();					// do all the gui stuff
	w.Swap();						// show what has just been drawn to the back buffer ( displayTexture + ImGui )
	FrameMark;						// tells tracy that this is the end of a frame
	return pQuit;					// break main loop when pQuit turns true
}

void engine::DrawAPIGeometry () {
	ZoneScoped;

	{
		scopedTimer Start( "API Geometry" );
		// draw some shit
	}
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
	ZoneScoped;

	scopedTimer( "Clear Color and Depth" );

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
	ZoneScoped;

	scopedTimer( "Blit to Screen" );

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
	ZoneScoped;

	ImguiFrameStart();						// start the imgui frame
	TonemapControlsWindow();

	// present the timing data
	timerQueries.gather();
	TimingReportWindow( timerQueries.queries );

	// data prep for LegitProfiler
	std::vector< legit::ProfilerTask > tasks;
	static ImGuiUtils::ProfilersWindow profilerWindow;
	float offset = 0;
	int color = 0;
	for ( auto& t : timerQueries.queries ) {
		legit::ProfilerTask pt;

		// calculate start and end times
		pt.startTime = offset / 1000.0f;
		offset = offset + t.result;
		pt.endTime = offset / 1000.0f;

		pt.name = t.label;
		pt.color = legit::Colors::colorList[ color++ ];
		color = color % legit::Colors::colorList.size();

		tasks.push_back( pt );
	}
	// add new profiling data and render
	profilerWindow.cpuGraph.LoadFrameData( &tasks[ 0 ], tasks.size() );
	profilerWindow.Render();
	// and we don't need the profiling information anymore
	timerQueries.clear();

	if ( true ) ImGui::ShowDemoWindow();	// show the demo window
	QuitConf( &quitConfirm );				// show quit confirm window, if triggered
	ImguiFrameEnd();						// finish up the imgui stuff and put it in the framebuffer
}

void engine::HandleEvents () {
	ZoneScoped;

	if ( !ImGui::GetIO().WantCaptureKeyboard ) {
		constexpr float bigStep = 0.120;
		constexpr float lilStep = 0.008;
		// can handle multiple simultaneous inputs like this
		const uint8_t *state = SDL_GetKeyboardState( NULL );
		// these will operate on the trident object, which retains state for block orientation
		if ( state[ SDL_SCANCODE_LEFT ] )
			trident.RotateY( ( SDL_GetModState() & KMOD_SHIFT ) ?  bigStep :  lilStep );
		if ( state[ SDL_SCANCODE_RIGHT ] )
			trident.RotateY( ( SDL_GetModState() & KMOD_SHIFT ) ? -bigStep : -lilStep );
		if ( state[ SDL_SCANCODE_UP ] )
			trident.RotateX( ( SDL_GetModState() & KMOD_SHIFT ) ?  bigStep :  lilStep );
		if ( state[ SDL_SCANCODE_DOWN ] )
			trident.RotateX( ( SDL_GetModState() & KMOD_SHIFT ) ? -bigStep : -lilStep );
		if ( state[ SDL_SCANCODE_PAGEUP ] )
			trident.RotateZ( ( SDL_GetModState() & KMOD_SHIFT ) ? -bigStep : -lilStep );
		if ( state[ SDL_SCANCODE_PAGEDOWN ] )
			trident.RotateZ( ( SDL_GetModState() & KMOD_SHIFT ) ?  bigStep :  lilStep );

		if ( state[ SDL_SCANCODE_1 ] )
			trident.SetViewFront();
		if ( state[ SDL_SCANCODE_2 ] )
			trident.SetViewRight();
		if ( state[ SDL_SCANCODE_3 ] )
			trident.SetViewBack();
		if ( state[ SDL_SCANCODE_4 ] )
			trident.SetViewLeft();
		if ( state[ SDL_SCANCODE_5 ] )
			trident.SetViewUp();
		if ( state[ SDL_SCANCODE_6 ] )
			trident.SetViewDown();

		// if ( trident.Dirty() ) // rotation or movement has happened
			// render.framesSinceLastInput = 0;

		if ( state[ SDL_SCANCODE_W ] ) {		cout << "W Pressed" << newline; }
		if ( state[ SDL_SCANCODE_S ] ) {		cout << "S Pressed" << newline; }
		if ( state[ SDL_SCANCODE_A ] ) {		cout << "A Pressed" << newline; }
		if ( state[ SDL_SCANCODE_D ] ) {		cout << "D Pressed" << newline; }
	}

//==============================================================================
// Need to keep this for pQuit handling ( force quit )
// In particular - checking for window close and the SDL_QUIT event can't really be determined
//  via the keyboard state, and then imgui needs it too, so can't completely kill the event
//  polling loop - maybe eventually I'll find a solution for this
	SDL_Event event;
	SDL_PumpEvents();
	while ( SDL_PollEvent( &event ) ) {
		// imgui event handling
		ImGui_ImplSDL2_ProcessEvent( &event );
		// swap out the multiple if statements for a big chained boolean setting the value of pQuit
		pQuit = ( event.type == SDL_QUIT ) ||
				( event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID( w.window ) ) ||
				( event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE && SDL_GetModState() & KMOD_SHIFT );
		// this has to stay because it doesn't seem like ImGui::IsKeyReleased is stable enough to use
		if ( ( event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE ) || ( event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_X1 )  )
			quitConfirm = !quitConfirm;
		if ( !ImGui::GetIO().WantCaptureMouse ) {
			ImVec2 valueRaw = ImGui::GetMouseDragDelta( 1, 0.0f );
			if ( ( valueRaw.x != 0 || valueRaw.y != 0 ) ) {
				trident.RotateY( -valueRaw.x * 0.03f );
				trident.RotateX( -valueRaw.y * 0.03f );
				ImGui::ResetMouseDragDelta( 1 );
			}
		}
	}
}

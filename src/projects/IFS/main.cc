#include "../../engine/engine.h"

class ifs final : public engineBase { // sample derived from base engine class
public:
	ifs () { Init(); OnInit(); PostInit(); }
	~ifs () { Quit(); }

	GLuint maxBuffer;

	// IFS view parameters
	float scale = 1.0f;
	vec2 offset = vec2( 0.0f );
	float rotation = 0.0f;

	// output prep
	float brightness = 1.0f;
	int paletteSelect = 13;

	// flag for field wipe (on zoom, drag, etc)
	bool bufferNeedsReset = false;

	void LoadShaders() {
		shaders[ "Draw" ] = computeShader( "./src/projects/IFS/shaders/draw.cs.glsl" ).shaderHandle;
		shaders[ "Update" ] = computeShader( "./src/projects/IFS/shaders/update.cs.glsl" ).shaderHandle;
		shaders[ "Clear" ] = computeShader( "./src/projects/IFS/shaders/clear.cs.glsl" ).shaderHandle;
	}

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			LoadShaders();

			// texture to accumulate into
			textureOptions_t opts;
			opts.width = config.width;
			opts.height = config.height;
			opts.dataType = GL_R32UI;
			opts.textureType = GL_TEXTURE_2D;
			textureManager.Add( "IFS Accumulator", opts );

			// buffer holding the normalize factor
			glGenBuffers( 1, &maxBuffer );
			constexpr uint32_t countValue = 0;
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, maxBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, 4, ( GLvoid * ) &countValue, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, maxBuffer );
		}
	}

	void HandleQuitEvents () {
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

			if ( event.type == SDL_MOUSEWHEEL && !ImGui::GetIO().WantCaptureMouse ) {
				// float wheel_x = -event.wheel.x;
				const float wheel_y = event.wheel.y;
				scale *= ( wheel_y > 0.0f ) ? wheel_y * ( ( SDL_GetModState() & KMOD_SHIFT ) ? 1.01f : 1.1f ) : abs( wheel_y ) * ( ( SDL_GetModState() & KMOD_SHIFT ) ? 0.99f : 0.9f );
				bufferNeedsReset = true;
			}
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// current state of the whole keyboard
		const uint8_t * state = SDL_GetKeyboardState( NULL );

		// // current state of the modifier keys
		const SDL_Keymod k	= SDL_GetModState();
		const bool shift		= ( k & KMOD_SHIFT );
		// const bool alt		= ( k & KMOD_ALT );
		// const bool control	= ( k & KMOD_CTRL );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

		// rotation controls (CW)
		if ( state[ SDL_SCANCODE_Q ] ) {
			bufferNeedsReset = true;
			rotation += shift ? 0.1f : 0.01f;
		}

		// rotation controls (CCW)
		if ( state[ SDL_SCANCODE_E ] ) {
			bufferNeedsReset = true;
			rotation -= shift ? 0.1f : 0.01f;
		}

		if ( state[ SDL_SCANCODE_Y ] && shift ) {
			// reload shaders
			LoadShaders();
		}

		ivec2 mouse;
		uint32_t mouseState = SDL_GetMouseState( &mouse.x, &mouse.y );
		if ( mouseState != 0 && !ImGui::GetIO().WantCaptureMouse ) {
			ImVec2 currentMouseDrag = ImGui::GetMouseDragDelta( 0 );
			ImGui::ResetMouseDragDelta();
			const float aspectRatio = ( float ) config.height / ( float ) config.width;
			offset.x += currentMouseDrag.x * aspectRatio * 0.01f / scale;
			offset.y += currentMouseDrag.y * 0.01f / scale;
			bufferNeedsReset = true;
		}
	}

	void ImguiPass () {
		ZoneScoped;
	
		ImGui::Begin( "Controls" );
		ImGui::SliderFloat( "Rotation", &rotation, -10.0f, 10.0f );
		bufferNeedsReset = bufferNeedsReset || ImGui::IsItemEdited();
		ImGui::SliderFloat( "Scale", &scale, 0.0f, 100.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
		bufferNeedsReset = bufferNeedsReset || ImGui::IsItemEdited();
		ImGui::SliderFloat( "X Offset", &offset.x, -100.0f, 100.0f );
		bufferNeedsReset = bufferNeedsReset || ImGui::IsItemEdited();
		ImGui::SliderFloat( "Y Offset", &offset.y, -100.0f, 100.0f );
		bufferNeedsReset = bufferNeedsReset || ImGui::IsItemEdited();
		ImGui::SliderFloat( "Brightness", &brightness, 0.0f,5000.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
		ImGui::SliderInt( "PaletteSelect", &paletteSelect, 0, 26 );
		ImGui::End();

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

		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );
			glUniform1f( glGetUniformLocation( shader, "brightness" ), brightness );
			glUniform1i( glGetUniformLocation( shader, "paletteSelect" ), paletteSelect );
			textureManager.BindImageForShader( "IFS Accumulator", "ifsAccumulator", shader, 2 );
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
			textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		static rngi wangSeeder = rngi( 0, 10000000 );

		// reset the buffer, when needed
		if ( bufferNeedsReset == true ) {
			bufferNeedsReset = false;

			// reset the value tracking the max
			constexpr uint32_t countValue = 0;
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, maxBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, 4, ( GLvoid * ) &countValue, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, maxBuffer );

			// reset the accumulator
			const GLuint shader = shaders[ "Clear" ];
			glUseProgram( shader );
			textureManager.BindImageForShader( "IFS Accumulator", "ifsAccumulator", shader, 2 );
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_ALL_BARRIER_BITS );
		}

		// application-specific update code
		ZoneScoped; scopedTimer Start( "Update" );
		const GLuint shader = shaders[ "Update" ];
		glUseProgram( shader );
		glUniform1i( glGetUniformLocation( shader, "wangSeed" ), wangSeeder() );
		glUniform1f( glGetUniformLocation( shader, "scale" ), scale );
		glUniform2f( glGetUniformLocation( shader, "offset" ), offset.x, offset.y );
		glUniform1f( glGetUniformLocation( shader, "rotation" ), rotation );
		textureManager.BindImageForShader( "IFS Accumulator", "ifsAccumulator", shader, 2 );
		glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
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
		OnUpdate();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	ifs engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

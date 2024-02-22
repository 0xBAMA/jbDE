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
			textureManager.Add( "Depth/Normals Accumulator", opts );
			textureManager.Add( "Color Accumulator", opts );
			textureManager.Add( "Color Accumulator Scratch", opts );
			textureManager.Add( "Tonemapped", opts );

		}

	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// // current state of the whole keyboard
		// const uint8_t * state = SDL_GetKeyboardState( NULL );

		// // current state of the modifier keys
		// const SDL_Keymod k	= SDL_GetModState();
		// const bool shift		= ( k & KMOD_SHIFT );
		// const bool alt		= ( k & KMOD_ALT );
		// const bool control	= ( k & KMOD_CTRL );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

		// float scrollAmount = ImGui::GetIO().MouseWheel;
		// daedalusConfig.outputZoom -= scrollAmount * 0.08f;

		int mouseX, mouseY;
		uint32_t mouseState = SDL_GetMouseState( &mouseX, &mouseY );
		if ( mouseState != 0 && !ImGui::GetIO().WantCaptureMouse ) {
			vec2 fractionalPosition = vec2( float( mouseX ) / float( daedalusConfig.targetWidth ), 1.0f - ( float( mouseY ) / float( daedalusConfig.targetHeight ) ) );
			ImVec2 currentMouseDrag = ImGui::GetMouseDragDelta( 0 );
			ImGui::ResetMouseDragDelta();
			const float aspectRatio = ( float ) daedalusConfig.targetHeight / ( float ) daedalusConfig.targetWidth;
			daedalusConfig.outputOffset.x -= currentMouseDrag.x * aspectRatio * daedalusConfig.outputZoom;
			daedalusConfig.outputOffset.y -= currentMouseDrag.y * daedalusConfig.outputZoom;
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
				const vec2 targetHalf = vec2( daedalusConfig.targetWidth, daedalusConfig.targetHeight ) / 2.0f;

				daedalusConfig.outputZoom -= wheel_y * ( ( SDL_GetModState() & KMOD_SHIFT ) ? 0.07f : 0.01f );
				daedalusConfig.outputZoom = std::clamp( daedalusConfig.outputZoom, 0.005f, 5.0f );

				// this is fuckin broken as shit

				vec2 previousCenter = ( previousOffset + targetHalf ) * previousZoom;
				daedalusConfig.outputOffset = previousCenter / daedalusConfig.outputZoom - targetHalf / daedalusConfig.outputZoom;

				// daedalusConfig.outputOffset -= ( previousOffset + targetHalf ) * previousZoom;
				// daedalusConfig.outputOffset += (  )

				// vec2 previousCenterPoint = targetHalf * daedalusConfig.outputZoom + previousOffset * daedalusConfig.outputZoom;
				// previousCenterPoint = previousCenterPoint / previousZoom;
				// daedalusConfig.outputOffset = previousCenterPoint - targetHalf * previousZoom;
			}
		}

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

	void DrawAPIGeometry () {
		ZoneScoped; scopedTimer Start( "API Geometry" );
		// draw some shit
	}

	void ComputePasses () {
		ZoneScoped;

		{
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );
			glUniform1f( glGetUniformLocation( shader, "scale" ), daedalusConfig.outputZoom );
			glUniform2f( glGetUniformLocation( shader, "offset" ), daedalusConfig.outputOffset.x, daedalusConfig.outputOffset.y );
			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );
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
		ZoneScoped; scopedTimer Start( "Update" );
		// application-specific update code
	}

	void OnRender () {
		ZoneScoped;
		ClearColorAndDepth();		// if I just disable depth testing, this can disappear
		DrawAPIGeometry();			// draw any API geometry desired
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

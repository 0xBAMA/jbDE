#include "../../../engine/engine.h"

class Icarus final : public engineBase { // sample derived from base engine class
public:
	Icarus () { Init(); OnInit(); PostInit(); }
	~Icarus () { Quit(); }

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			shaders[ "Draw" ] = computeShader( "./src/projects/PathTracing/Icarus/shaders/draw.cs.glsl" ).shaderHandle;

		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );


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

			glUniform2i( glGetUniformLocation( shader, "clickPush" ), inputHandler.clickPush.x, inputHandler.clickPush.y );
			glUniform2i( glGetUniformLocation( shader, "currentMouse" ), inputHandler.clickPush.x + inputHandler.mouseDragDelta().x, inputHandler.clickPush.y + inputHandler.mouseDragDelta().y );

			if ( inputHandler.dragging ) {
				cout << "delta is " << inputHandler.mouseDragDelta().x << " " << inputHandler.mouseDragDelta().y << endl;
			}

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

int main ( int argc, char *argv[] ) {
	Icarus engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

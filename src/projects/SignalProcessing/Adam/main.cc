#include "../../../engine/engine.h"

class Adam final : public engineBase { // sample derived from base engine class
public:
	Adam () { Init(); OnInit(); PostInit(); }
	~Adam () { Quit(); }

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Draw" ] = computeShader( "./src/projects/SignalProcessing/Adam/shaders/draw.cs.glsl" ).shaderHandle;

			int w = 1024;
			int h = 1024;

			// creating a square image
			textureOptions_t opts;
			opts.textureType = GL_TEXTURE_2D;
			opts.minFilter = GL_NEAREST_MIPMAP_NEAREST;
			opts.magFilter = GL_NEAREST;
			opts.width = w;
			opts.height = h;
			opts.dataType = GL_R32F;
			textureManager.Add( "Adam", opts );

			// Image_4U zeroes( w, h );
			// zeroes.ClearTo( Image_4U::color( { 1, 1, 1, 1 } ) );
			// void * data = ( void * ) zeroes.GetImageDataBasePtr();
			// glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Adam" ) );

			// int level = 0;
			// while ( h >= 1 ) {
			// 	// we half on both dimensions at once
			// 	h /= 2;
			// 	w /= 2;
			// 	level++;
			// 	glTexImage2D( GL_TEXTURE_2D, level, opts.dataType, w, h, 0, getFormat( opts.dataType ), GL_UNSIGNED_BYTE, data );
			// }
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// get new data into the input handler
		inputHandler.update();

		// pass any signals into the terminal
		terminal.update( inputHandler );
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

		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );
			textureManager.BindTexForShader( "Adam", "image", shader, 2 );

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
	Adam engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

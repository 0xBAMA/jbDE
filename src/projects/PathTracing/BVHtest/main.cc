#include "../../../engine/engine.h"
#include "bvh.h"

class BVHtest final : public engineBase { // sample derived from base engine class
public:
	BVHtest () { Init(); OnInit(); PostInit(); }
	~BVHtest () { Quit(); }

	testRenderer_t renderer;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Draw" ] = computeShader( "./src/projects/PathTracing/BVHtest/shaders/draw.cs.glsl" ).shaderHandle;

			// get some random triangles
			// renderer.init();

			// create the image buffer for GPU display
			textureOptions_t opts;
			opts.width			= renderer.imageBuffer.Width();
			opts.height			= renderer.imageBuffer.Height();
			opts.dataType		= GL_RGBA32F;
			opts.minFilter		= GL_LINEAR;
			opts.magFilter		= GL_LINEAR;
			opts.textureType	= GL_TEXTURE_2D;
			opts.wrap			= GL_CLAMP_TO_BORDER;
			opts.pixelDataType	= GL_FLOAT;
			opts.initialData	= ( void * ) renderer.imageBuffer.GetImageDataBasePtr();
			textureManager.Add( "Image Buffer", opts );

			// == Load the Model =====================================================================
			terminal.addCommand( { "LoadModel" }, {},
				[=] ( args_t args ) {
					// load the model

				}, "Load the model from disk." );

			// == Build the BVH ======================================================================
			terminal.addCommand( { "BuildBVH" }, {{ "mode", INT, "Select BVH Construction Method." }},
				[=] ( args_t args ) {

					// use the loaded model to build a bvh
					switch( args[ "mode" ] ) {
						case 0: // naiive
							break;

						case 1: // SAH
							break;

						case 2: // Binned SAH
							break;

						default: break;
					}
					// report how long it took

				}, "Build the BVH from the currently loaded model." );
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// new data into the input handler
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
			// glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );
			textureManager.BindImageForShader( "Image Buffer", "CPUTexture", shader, 2 );
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
	BVHtest engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

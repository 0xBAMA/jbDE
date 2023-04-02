#include "../../engine/engine.h"
#include "model.h"

class engineChild : public engineBase {	// example derived class
public:
	engineChild () { Init(); OnInit(); PostInit(); }
	~engineChild () { Quit(); }

	// SoftBody Simulation Model
	model simulationModel;

	void OnInit () {
		ZoneScoped;
		{ Block Start( "Additional User Init" );
			// this will also contain application specific textures, shaders, and bindsets

			// compile background shader
			shaders[ "Background" ] = computeShader( "./src/projects/SoftBodies/shaders/background.cs.glsl" ).shaderHandle;

			// compile display shaders
			simulationModel.simGeometryShader =
				shaders[ "Softbody Display" ] = regularShader( "./src/projects/SoftBodies/shaders/main.vs.glsl", "./src/projects/SoftBodies/shaders/main.fs.glsl" ).shaderHandle;

			// load and initialize the model
			simulationModel.loadFramePoints();
			simulationModel.GPUSetup();
			simulationModel.passNewGPUData();
		}
	}

	void HandleCustomEvents () {
		ZoneScoped;
		// application specific controls
	}

	void ImguiPass () {
		ZoneScoped;

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		simulationModel.Update(); // update the CPU model
	}

	void DrawAPIGeometry () {
		ZoneScoped; scopedTimer Start( "API Geometry" );
		simulationModel.Display(); // draw the raster geometry
	}

	void ComputePasses () {
		ZoneScoped;

		// potentially doing the GPU model update here

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Background" ] );
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
			textRenderer.Draw( textures[ "Display Texture" ] );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // show trident with current orientation
			scopedTimer Start( "Trident" );
			trident.Update( textures[ "Display Texture" ] );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnRender () {
		ZoneScoped;
		ClearColorAndDepth();		// clear the buffer
		ComputePasses();			// draw the background + timing data
		BlitToScreen();				// fullscreen triangle copying to the screen
		DrawAPIGeometry();			// draw the model
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

		{ scopedTimer Start( "Handle Events" );
			HandleTridentEvents();
			HandleCustomEvents();
			HandleQuitEvents();
		}

		OnUpdate();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	engineChild engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

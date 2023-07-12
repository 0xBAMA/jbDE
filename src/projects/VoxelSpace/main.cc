#include "../../engine/engine.h"

struct VoxelSpaceConfig_t {

	// what sim do we want to run
	int32_t mode;

	// renderer config
		// tbd

	// thread sync, erosion control
	bool threadShouldRun;
	bool erosionReady;

};

class VoxelSpace : public engineBase {	// example derived class
public:
	VoxelSpace () { Init(); OnInit(); PostInit(); }
	~VoxelSpace () { Quit(); }

	VoxelSpaceConfig_t voxelSpaceConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// load config
			json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();
			voxelSpaceConfig.mode = j[ "app" ][ "VoxelSpace" ][ "mode" ];

			// compile all the shaders

			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Dummy Draw" ] = computeShader( "./src/projects/VoxelSpace/shaders/dummyDraw.cs.glsl" ).shaderHandle;

			// rendering shader

			// stuff that always runs
				// create the texture for the heightmap / colormap ( pack as 4 channel, rgb + height )
				// create the texture for the regular display
				// create the texture for the minimap

			if ( voxelSpaceConfig.mode == -1 ) { // we know that we want to run the live erosion sim

				// create the heightmap that's used for the erosion

				// load that initial heightmap into the map display texture

				// set threadShouldRun flag, once everything is configured
				// unset erosionReady flag, since that data is now potentially in flux

			}
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );
		// const uint8_t * state = SDL_GetKeyboardState( NULL );
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

	void DrawAPIGeometry () {
		ZoneScoped; scopedTimer Start( "API Geometry" );
		// draw some shit
	}

	void ComputePasses () {
		ZoneScoped;

		{
			// dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Dummy Draw" ] );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "time" ), SDL_GetTicks() / 1600.0f );
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

			if ( voxelSpaceConfig.mode == -1 ) {
				// check to see if the erosion thread is ready
					// if it is
						// pass it to the image
						// run the "shade" shader

				// barrier
			}
		
			// draw the regular map into its image

			// draw the minimap into its image

			// flatten it down, put it in the accumulator - we're not going to screw with a framebuffer for this, I don't think...
				// tbd, might be easier than writing the alpha blending myself, to use the API blending

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
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // show trident with current orientation
			scopedTimer Start( "Trident" );
			trident.Update( textureManager.Get( "Display Texture" ) );
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
		HandleTridentEvents();
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
	VoxelSpace VoxelSpaceInstance;
	while( !VoxelSpaceInstance.MainLoop() );
	return 0;
}

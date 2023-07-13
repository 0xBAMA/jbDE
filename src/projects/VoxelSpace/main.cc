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
			shaders[ "Background" ] = computeShader( "./src/projects/VoxelSpace/shaders/Background.cs.glsl" ).shaderHandle;
			shaders[ "VoxelSpace" ] = computeShader( "./src/projects/VoxelSpace/shaders/VoxelSpace.cs.glsl" ).shaderHandle;

			// create the texture for the landscape
			if ( voxelSpaceConfig.mode == 0 ) {
				// we know that we want to run the live erosion sim

				// create the initial heightmap that's used for the erosion

				// create the texture from that heightmap
				textureOptions_t opts;
				opts.width = config.width;
				opts.height = config.height;
				opts.textureType = GL_TEXTURE_2D;


				// set threadShouldRun flag, since once everything is configured, the thread should run
				voxelSpaceConfig.threadShouldRun = true;
				// unset erosionReady flag, since that data is now potentially in flux
				voxelSpaceConfig.erosionReady = false;

			} else if ( voxelSpaceConfig.mode >= 1 && voxelSpaceConfig.mode <= 30 ) {
				// we want to load one of the basic maps from disk - color in the rgb + height in alpha
				Image_4U map( string( "./src/projects/VoxelSpace/data/map" ) + std::to_string( voxelSpaceConfig.mode ) + string( ".png" ) );

				// create the texture from the loaded image
				textureOptions_t opts;
				opts.width = map.Width();
				opts.height = map.Height();
				opts.textureType = GL_TEXTURE_2D;
				// ...

			} else {
				cout << "invalid mode selected" << newline;
				abort();
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
			// dummy draw - maybe revive this for doing some kind of background
			// scopedTimer Start( "Drawing" );
			// bindSets[ "Drawing" ].apply();
			// glUseProgram( shaders[ "Dummy Draw" ] );
			// glUniform1f( glGetUniformLocation( shaders[ "Background" ], "time" ), SDL_GetTicks() / 1600.0f );
			// glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

			if ( voxelSpaceConfig.mode == -1 ) {
				// check to see if the erosion thread is ready
					// if it is
						// pass it to the image
						// run the "shade" shader

				// barrier
			}

			// draw the regular map into the accumulator

			// draw the minimap into the accumulator

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
		// BlitToScreen();				// fullscreen triangle copying to the screen
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
	VoxelSpace VoxelSpaceInstance;
	while( !VoxelSpaceInstance.MainLoop() );
	return 0;
}

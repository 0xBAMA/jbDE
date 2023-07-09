#include "../../engine/engine.h"

struct CAConfig_t {
	uint32_t dimensionX;
	uint32_t dimensionY;
	bool oddFrame = false;
};

class engineDemo : public engineBase {	// example derived class
public:
	engineDemo () { Init(); OnInit(); PostInit(); }
	~engineDemo () { Quit(); }

	CAConfig_t CAConfig;

	void OnInit () {
		ZoneScoped;
		{ Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			shaders[ "Draw" ] = computeShader( "./src/projects/CellularAutomata/shaders/draw.cs.glsl" ).shaderHandle;
			shaders[ "Update" ] = computeShader( "./src/projects/CellularAutomata/shaders/update.cs.glsl" ).shaderHandle;

			json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();
			CAConfig.dimensionX = j[ "app" ][ "CellularAutomata" ][ "dimensionX" ];
			CAConfig.dimensionY = j[ "app" ][ "CellularAutomata" ][ "dimensionY" ];

		// setup the image buffers for the CA state ( 2x for ping-ponging )
			// random data init
			std::vector< uint8_t > initialData;
			rngi gen( 0, 1 );
			for ( size_t i = 0; i < CAConfig.dimensionX * CAConfig.dimensionY; i++ )
				initialData.push_back( gen() );

			textureOptions_t opts;
			opts.dataType		= GL_R32UI;
			opts.width			= CAConfig.dimensionX;
			opts.height			= CAConfig.dimensionY;
			opts.textureType	= GL_TEXTURE_2D;
			opts.pixelDataType	= GL_UNSIGNED_BYTE;
			opts.initialData	= ( void * ) initialData.data();
			textureManager.Add( "Automata State Buffer 0", opts );
			textureManager.Add( "Automata State Buffer 1", opts );
		}
	}

	void HandleCustomEvents () {
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// reset buffer contents, in the back buffer

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
	}

	void ComputePasses () {
		ZoneScoped;

		// swap buffers - calculate strings for use later
		CAConfig.oddFrame = !CAConfig.oddFrame;
		string backBufferLabel = string( "Automata State Buffer " ) + string( CAConfig.oddFrame ? "0" : "1" );
		string frontBufferLabel = string( "Automata State Buffer " ) + string( CAConfig.oddFrame ? "1" : "0" );

		{ // update the state of the CA
			scopedTimer Start( "Update" );
			glUseProgram( shaders[ "Update" ] );

			// bind front buffer, back buffer
			textureManager.BindImageForShader( backBufferLabel, "backBuffer", shaders[ "Update" ], 2 );
			textureManager.BindImageForShader( frontBufferLabel, "frontBuffer", shaders[ "Update" ], 3 );

			// dispatch the compute shader - go from back buffer to front buffer
			glDispatchCompute( ( CAConfig.dimensionX + 15 ) / 16, ( CAConfig.dimensionY + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // draw the current state of the front buffer
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Draw" ] );

			// pass the current front buffer, to display it
			glUniform2f( glGetUniformLocation( shaders[ "Draw" ], "resolution" ), CAConfig.dimensionX, CAConfig.dimensionY );
			textureManager.BindTexForShader( frontBufferLabel, "CAStateBuffer", shaders[ "Draw" ], 2 );

			// put it in the accumulator
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
		HandleTridentEvents();
		HandleCustomEvents();
		HandleQuitEvents();

		// derived-class-specific functionality
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	engineDemo engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

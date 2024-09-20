#include "../../../engine/engine.h"

struct CantorDustConfig_t {
	uint32_t maxValue = 0;
	float power = 1.0f;
};

class CantorDust final : public engineBase { // sample derived from base engine class
public:
	CantorDust () { Init(); OnInit(); PostInit(); }
	~CantorDust () { Quit(); }

	CantorDustConfig_t CantorDustConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Draw" ] = computeShader( "./src/projects/SignalProcessing/CantorDust/shaders/draw.cs.glsl" ).shaderHandle;

		// processing the data
			// setting up and initializing the buffer for the histogram
			std::vector< uint32_t > histogram;
			histogram.resize( 256 * 256, 0 );

		// consider wrapping the below in a recursive filesystem iteration... do it for all the files in a folder + subfolders?
			// const string filename = string( "./bin/CantorDust" );
			// const string filename = string( "./TODO.md" );
			const string filename = string( "../../Downloads/544141-1-2004-ranger-ford-stock-stock-cragar-soft-8-black.webp" );
			// const string filename = string( "../PolynomialOptics-Source.zip" );
			// const string filename = string( "./src/projects/SignalProcessing/CantorDust/main.cc" );
			// const string filename = string( "../EXRs/mosaic_tunnel_4k.exr" );

			std::vector< unsigned char > bytes;
			bytes.reserve( std::filesystem::file_size( std::filesystem::path( filename ) ) );
			std::ifstream file( filename, ios_base::in | ios_base::binary );
			bytes.insert( bytes.end(), istreambuf_iterator< char >{ file }, {} );

			// now we're going to step throught the bytes, and populate the histogram
			for ( size_t idx = 0; idx < ( bytes.size() - 1 ); idx++ ) {
				// load the current and next bytes...
				uint8_t current	= bytes[ idx ];
				uint8_t next	= bytes[ idx + 1 ];
				// increment the corresponding histogram bin
				histogram[ current + next * 256 ] += 1;
			}

			// determine the maximum across the histogram, to normalize
			for ( size_t idx = 0; idx < ( histogram.size() - 1 ); idx++ ) {
				CantorDustConfig.maxValue = std::max( histogram[ idx ], CantorDustConfig.maxValue );
			}

			// setup a texture to hold the data
			textureOptions_t opts;
			opts.dataType		= GL_R32UI;
			opts.width			= 256;
			opts.height			= 256;
			opts.textureType	= GL_TEXTURE_2D;
			opts.pixelDataType	= GL_UNSIGNED_INT;
			opts.initialData	= ( void * ) histogram.data();
			textureManager.Add( "Data Texture", opts );
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

			glUniform1ui( glGetUniformLocation( shader, "maxValue" ), CantorDustConfig.maxValue );
			glUniform1f( glGetUniformLocation( shader, "power" ), CantorDustConfig.power );
			textureManager.BindImageForShader( "Data Texture", "dataTexture", shader, 2 );

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
	CantorDust engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

#include "../../../engine/engine.h"

struct CAConfig_t {
	// CA buffer dimensions
	uint32_t dimensionX;
	uint32_t dimensionY;

	// deciding how to populate the buffer
	float generatorThreshold;

	// toggling buffers
	bool oddFrame = false;

	int rule[ 25 ] = {
		2, 0, 2, 2, 1,
		0, 2, 0, 2, 2,
		1, 1, 0, 0, 2,
		0, 1, 1, 2, 1,
		2, 2, 1, 1, 2
	};
};

class CAHistory final : public engineBase {
public:
	CAHistory () { Init(); OnInit(); PostInit(); }
	~CAHistory () { Quit(); }

	CAConfig_t CAConfig;

	void newRule () {
		rngi gen( 0, 2 );
		for ( int i = 0; i < 25; i++ ) {
			CAConfig.rule[ i ] = gen();
		}
	}

	void newRuleM () {
		rngi offset( 0, 24 );
		rngi gen( 0, 2 );

		CAConfig.rule[ offset() ] = gen();
	}

	void OnInit () {
		ZoneScoped;
		{ Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			ReloadShaders();

			json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();
			CAConfig.dimensionX = j[ "app" ][ "CellularAutomata" ][ "dimensionX" ];
			CAConfig.dimensionY = j[ "app" ][ "CellularAutomata" ][ "dimensionY" ];
			CAConfig.generatorThreshold = j[ "app" ][ "CellularAutomata" ][ "generatorThreshold" ];

		// setup the image buffers for the CA state ( 2x for ping-ponging )
			textureOptions_t opts;
			opts.dataType		= GL_R32UI;
			opts.width			= CAConfig.dimensionX;
			opts.height			= CAConfig.dimensionY;
			opts.textureType	= GL_TEXTURE_2D;
			opts.pixelDataType	= GL_UNSIGNED_BYTE;
			opts.initialData	= nullptr;
			textureManager.Add( "Automata State Buffer 0", opts );
			textureManager.Add( "Automata State Buffer 1", opts );

			BufferReset();
		}
	}

	void ReloadShaders () {
		shaders[ "Draw" ] = computeShader( "./src/projects/CellularAutomata/table/shaders/draw.cs.glsl" ).shaderHandle;
		shaders[ "Update" ] = computeShader( "./src/projects/CellularAutomata/table/shaders/update.cs.glsl" ).shaderHandle;
	}

	void BufferReset () { // put random bits in the buffer
		string backBufferLabel = string( "Automata State Buffer " ) + string( CAConfig.oddFrame ? "0" : "1" );

		// random data init
		std::vector< uint32_t > initialData;
		rng gen( 0.0f, 1.0f );
		// for ( size_t i = 0; i < CAConfig.dimensionX * CAConfig.dimensionY; i++ ) {
		// 	uint32_t value = 0;
		// 	for ( size_t b = 0; b < 32; b++ ) {
		// 		value = value << 1;
		// 		value = value | ( ( gen() < CAConfig.generatorThreshold ) ? 1u : 0u );
		// 	}
		// 	initialData.push_back( value );
		// }

		const uint dX = CAConfig.dimensionX;
		const uint dY = CAConfig.dimensionY;

		for ( uint y = 0; y < CAConfig.dimensionY; y++ ) {
			for ( uint x = 0; x < CAConfig.dimensionX; x++ ) {
				// if ( gen() < ( distance( vec2( CAConfig.dimensionX / 2.0f, CAConfig.dimensionY / 2.0f ), vec2( x, y ) ) / 1000.0f ) ) {gr
				// 	initialData.push_back( 1 );
				// } else {
				// 	initialData.push_back( 0 );
				// }

				initialData.push_back( ( x > 100 && y > 100 && x < dX - 100 && y < dY - 100 ) ? ( gen() < CAConfig.generatorThreshold ? 1 : 0 ) : 0 );
			}
		}

		// no current functionality for updating the buffer - going to raw OpenGL
		GLuint handle = textureManager.Get( backBufferLabel );
		glBindTexture( GL_TEXTURE_2D, handle );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_R32UI, CAConfig.dimensionX, CAConfig.dimensionY, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, ( void * ) initialData.data() );
	}

	void HandleCustomEvents () {
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		const uint8_t * state = SDL_GetKeyboardState( NULL );
		if ( state[ SDL_SCANCODE_R ] ) {
			// reset buffer contents, in the back buffer
			BufferReset();
			SDL_Delay( 20 ); // debounce
		}
	
		if ( state[ SDL_SCANCODE_Y ] ) {
			ReloadShaders();
		}

		if ( state[ SDL_SCANCODE_G ] ) {
			newRule();
		}

		if ( state[ SDL_SCANCODE_H ] ) {
			newRuleM();
		}
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

		// swap buffers - precalculate strings for use later
		string backBufferLabel = string( "Automata State Buffer " ) + string( CAConfig.oddFrame ? "0" : "1" );
		string frontBufferLabel = string( "Automata State Buffer " ) + string( CAConfig.oddFrame ? "1" : "0" );

		{ // update the state of the CA
			scopedTimer Start( "Update" );
			glUseProgram( shaders[ "Update" ] );

			// bind front buffer, back buffer
			textureManager.BindImageForShader( backBufferLabel, "backBuffer", shaders[ "Update" ], 2 );
			textureManager.BindImageForShader( frontBufferLabel, "frontBuffer", shaders[ "Update" ], 3 );

			glUniform1iv( glGetUniformLocation( shaders[ "Update" ], "rule" ), 25, &CAConfig.rule[ 0 ] );

			// dispatch the compute shader - go from back buffer to front buffer
			glDispatchCompute( ( CAConfig.dimensionX + 15 ) / 16, ( CAConfig.dimensionY + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // draw the current state of the front buffer
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Draw" ] );

			// pass the current front buffer, to display it
			glUniform2f( glGetUniformLocation( shaders[ "Draw" ], "resolution" ),
				( float( config.width ) / float( CAConfig.dimensionX ) ) * float( CAConfig.dimensionX ),
				( float( config.height ) / float( CAConfig.dimensionY ) ) * float( CAConfig.dimensionY ) );
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

			for ( int i = 0; i < 5; i++ ) {
				string str;
				for ( int j = 0; j < 5; j++ ) {
					str += " " + to_string( CAConfig.rule[ i * 5 + j ] );
				}
				textRenderer.DrawBlackBackedString( 6 - i, str + " " );
			}

			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		// buffer swap
		CAConfig.oddFrame = !CAConfig.oddFrame;
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
	CAHistory engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

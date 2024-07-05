#include "../../../engine/engine.h"

class CAHistory final : public engineBase {
public:
	CAHistory () { Init(); OnInit(); PostInit(); }
	~CAHistory () { Quit(); }

	const uvec3 dims = uvec3( 256, 256, 256 );
	vec3 viewerPosition = vec3( 1.0f, 1.0f, 1.0f );
	int currentSlice = 0;

	void OnInit () {
		ZoneScoped;
		{ Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			shaders[ "Draw" ] = computeShader( "./src/projects/CellularAutomata/CAHistory3D/shaders/draw.cs.glsl" ).shaderHandle;
			shaders[ "Update" ] = computeShader( "./src/projects/CellularAutomata/CAHistory3D/shaders/update.cs.glsl" ).shaderHandle;

			// setup the image buffers for the CA state ( 2x for ping-ponging )
			textureOptions_t opts;
			opts.dataType		= GL_R8UI;
			opts.width			= dims.x;
			opts.height			= dims.y;
			opts.depth			= dims.z;
			opts.textureType	= GL_TEXTURE_3D;
			opts.pixelDataType	= GL_UNSIGNED_BYTE;
			opts.initialData	= nullptr;
			textureManager.Add( "State Buffer", opts );

			BufferReset();
		}
	}

	void BufferReset () { // put random bits in the buffer
		// todo
	}

	void HandleCustomEvents () {
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		const uint8_t * state = SDL_GetKeyboardState( NULL );
		if ( state[ SDL_SCANCODE_R ] ) {
			// reset buffer contents, in the back buffer
			BufferReset();
			SDL_Delay( 20 ); // debounce
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

		// add some config stuff

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void ComputePasses () {
		ZoneScoped;

		{ // update the state of the CA
			scopedTimer Start( "Update" );
			// glUseProgram( shaders[ "Update" ] );

			// dispatch the compute shader to update the thing
			// glDispatchCompute( ( CAConfig.dimensionX + 15 ) / 16, ( CAConfig.dimensionY + 15 ) / 16, 1 );
			// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // draw the current state of the front buffer
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();

			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			glUniform3fv( glGetUniformLocation( shader, "viewerPosition" ), 1, glm::value_ptr( viewerPosition ) );

			// pass the current front buffer, to display it
			// glUniform2f( glGetUniformLocation( shaders[ "Draw" ], "resolution" ),
			// 	( float( config.width ) / float( CAConfig.dimensionX ) ) * float( CAConfig.dimensionX ),
			// 	( float( config.height ) / float( CAConfig.dimensionY ) ) * float( CAConfig.dimensionY ) );
			// textureManager.BindTexForShader( frontBufferLabel, "CAStateBuffer", shaders[ "Draw" ], 2 );

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
	CAHistory engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

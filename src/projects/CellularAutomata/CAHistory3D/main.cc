#include "../../../engine/engine.h"

class CAHistory final : public engineBase {
public:
	CAHistory () { Init(); OnInit(); PostInit(); }
	~CAHistory () { Quit(); }

	const uvec3 dims = uvec3( 512 );
	vec3 viewerPosition = vec3( 6.0f, 6.0f, 6.0f );
	int currentSlice = 0;

	bool userClickedThisFrame = false;
	ivec2 userClickLocation = ivec2( -1 );

	float zoom = 2.0f;
	float verticalOffset = 2.0f;

	void IncrementSlice() {
		currentSlice = ( currentSlice + 1 ) % dims.z;
		// cout << currentSlice << endl;
	}

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

			BufferReset( 0.5f );
		}
	}

	void BufferReset ( float onChance ) { // put random bits in the buffer
		static rng pick = rng( 0.0f, 1.0f );
		std::vector< uint8_t > data;
		glBindTexture( GL_TEXTURE_3D, textureManager.Get( "State Buffer" ) );

		// fill the block with zeroes
		data.resize( dims.x * dims.y, 0 );
		for ( uint i = 0; i < dims.z; i++ ) {
			glTexSubImage3D( GL_TEXTURE_3D, 0, 0, 0, i, dims.x, dims.y, 1, GL_RED_INTEGER, GL_UNSIGNED_BYTE, ( void * ) &data[ 0 ] );
		}

		// fill the first slice with noise
		for ( uint i = 0; i < dims.x * dims.y; i++ ) {
			data[ i ] = ( pick() < onChance ) ? 0 : 255;
		}
		currentSlice = 0;
		glTexSubImage3D( GL_TEXTURE_3D, 0, 0, 0, dims.z - 1, dims.x, dims.y, 1, GL_RED_INTEGER, GL_UNSIGNED_BYTE, ( void * ) &data[ 0 ] );
	}

	void HandleCustomEvents () {
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		const uint8_t * state = SDL_GetKeyboardState( NULL );
		if ( state[ SDL_SCANCODE_R ] ) {
			// reset buffer contents, in the back buffer
			BufferReset( 0.5f );
			SDL_Delay( 20 ); // debounce
		}

		if ( state[ SDL_SCANCODE_T ] ) {
			IncrementSlice();
			SDL_Delay( 100 ); // debounce
		}

		if ( state[ SDL_SCANCODE_RIGHT ] ) {
			glm::quat rot = glm::angleAxis( -0.01f, vec3( 0.0f, 0.0f, 1.0f ) );
			viewerPosition = ( rot * vec4( viewerPosition, 0.0f ) ).xyz();
		}

		if ( state[ SDL_SCANCODE_LEFT ] ) {
			glm::quat rot = glm::angleAxis( 0.01f, vec3( 0.0f, 0.0f, 1.0f ) );
			viewerPosition = ( rot * vec4( viewerPosition, 0.0f ) ).xyz();
		}

		if ( state[ SDL_SCANCODE_UP ] ) {
			verticalOffset += 0.1f;
		}

		if ( state[ SDL_SCANCODE_DOWN ] ) {
			verticalOffset -= 0.1f;
		}

		uint32_t mouseState = SDL_GetMouseState( &userClickLocation.x, &userClickLocation.y );
		if ( mouseState != 0 && !ImGui::GetIO().WantCaptureMouse ) {
			userClickedThisFrame = true;
		}

		// zoom in and out with plus/minus
		if ( state[ SDL_SCANCODE_MINUS ] ) {
			zoom += 0.1f;
		}

		if ( state[ SDL_SCANCODE_EQUALS ] ) {
			zoom -= 0.1f;
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

		{ // draw the current state of the front buffer
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();

			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			glUniform1f( glGetUniformLocation( shader, "zoom" ), zoom );
			glUniform1f( glGetUniformLocation( shader, "verticalOffset" ), verticalOffset );
			glUniform3fv( glGetUniformLocation( shader, "viewerPosition" ), 1, glm::value_ptr( viewerPosition ) );
			glUniform1i( glGetUniformLocation( shader, "sliceOffset" ), currentSlice );
			textureManager.BindImageForShader( "State Buffer", "CAStateBuffer", shader, 2 );

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

	void OnUpdate () {
		// update the state of the CA
		scopedTimer Start( "Update" );
		const GLuint shader = shaders[ "Update" ];
		glUseProgram( shader );

		glUniform1i( glGetUniformLocation( shader, "sliceOffset" ), currentSlice );
		glUniform3fv( glGetUniformLocation( shader, "viewerPosition" ), 1, glm::value_ptr( viewerPosition ) );
		textureManager.BindImageForShader( "State Buffer", "CAStateBuffer", shader, 2 );
		IncrementSlice();

		// if ( userClickedThisFrame == true ) {
			// 	// this probably makes more sense to do on the CPU...
			// 	// glUniform1i( glGetUniformLocation( shader, "userClickedThisFrame" ), userClickedThisFrame )

			// reset click state
			// userClickedThisFrame = false;
		// }

		// dispatch the compute shader to update the thing
		glDispatchCompute( dims.x / 16, dims.y / 16, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
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
		OnUpdate();
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

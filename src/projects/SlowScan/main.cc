#include "../../engine/engine.h"

class slowScan final : public engineBase { // sample derived from base engine class
public:
	slowScan () { Init(); OnInit(); PostInit(); }
	~slowScan () { DeInit(); Quit(); }

	static constexpr int N = 512;
	fftw_complex *inputData, *outputData;
	fftw_plan p;

	GLuint signalBuffer;
	GLuint fftBuffer;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Draw" ] = computeShader( "./src/projects/SlowScan/shaders/draw.cs.glsl" ).shaderHandle;

			// init fftw3 resources
			inputData = ( fftw_complex* ) fftw_malloc( sizeof( fftw_complex ) * N );
			outputData = ( fftw_complex* ) fftw_malloc( sizeof( fftw_complex ) * N );
			p = fftw_plan_dft_1d( N, inputData, outputData, FFTW_FORWARD, FFTW_ESTIMATE );

			// declare buffers, to pass signal + fft to GPU
			glGenBuffers( 1, &signalBuffer );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, signalBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * N, ( GLvoid * ) &inputData, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, signalBuffer );

			glGenBuffers( 1, &fftBuffer );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, fftBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * N, ( GLvoid * ) &outputData, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, fftBuffer );
		}
	}

	void DeInit () {
		fftw_destroy_plan( p );
		fftw_free( inputData );
		fftw_free( outputData );
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// // current state of the whole keyboard
		// const uint8_t * state = SDL_GetKeyboardState( NULL );

		// // current state of the modifier keys
		// const SDL_Keymod k	= SDL_GetModState();
		// const bool shift		= ( k & KMOD_SHIFT );
		// const bool alt		= ( k & KMOD_ALT );
		// const bool control	= ( k & KMOD_CTRL );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

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
		// draw some shit - need to add a hello triangle to this, so I have an easier starting point for raster stuff
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Draw" ] );
			glUniform1f( glGetUniformLocation( shaders[ "Draw" ], "time" ), SDL_GetTicks() / 1600.0f );
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
			// scopedTimer Start( "Trident" );
			// trident.Update( textureManager.Get( "Display Texture" ) );
			// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );

		static int offset = 0;
		offset++;

		// fill out the array with some shit
		for ( int i = 0; i < N; i++ ) {
			inputData[ i ][ 0 ] = sin( 0.1f * ( i + offset ) );
			inputData[ i ][ 1 ] = 0.0f;
		}

		fftw_execute( p );

		// put it in the buffer for the shader to read ( note use of double precision on CPU, single on GPU )
		static float inputDataCastToSinglePrecision[ N ] = { 0.0f };
		static float outputDataCastToSinglePrecision[ N ] = { 0.0f };

		for ( int i = 0; i < N; i++ ) {
			inputDataCastToSinglePrecision[ i ] = static_cast< float > ( inputData[ i ][ 0 ] );

			// need to get magnitude from the complex numbers, here - angle is phase
			outputDataCastToSinglePrecision[ i ] = static_cast< float > (
				sqrt( outputData[ i ][ 0 ] * outputData[ i ][ 0 ] + outputData[ i ][ 1 ] * outputData[ i ][ 1 ] ) );
		}

		glBindBuffer( GL_SHADER_STORAGE_BUFFER, signalBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * N, ( GLvoid * ) &inputDataCastToSinglePrecision, GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, signalBuffer );

		glBindBuffer( GL_SHADER_STORAGE_BUFFER, fftBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * N, ( GLvoid * ) &outputDataCastToSinglePrecision, GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, fftBuffer );
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
	slowScan engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

#include "../../engine/engine.h"

class slowScan final : public engineBase { // sample derived from base engine class
public:
	slowScan () { Init(); OnInit(); PostInit(); }
	~slowScan () { DeInit(); Quit(); }

	static constexpr int N = 2048;
	fftw_complex *inputData, *outputData;
	fftw_plan p;

	// holding the audio data, pulling out 800 samples per frame
		// ( 48000 samples per second / 60 frames per second = 800 samples per frame )
	SDL_AudioStream * streamBufferAnalyze = NULL;

	GLuint signalBuffer;
	GLuint fftBuffer;

	int waterfallRowUpdate = 0;
	const int waterfallHeight = 1024;

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
			glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * N, NULL, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, signalBuffer );

			glGenBuffers( 1, &fftBuffer );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, fftBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * N, NULL, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, fftBuffer );

			// waterfall graph, spectrogram - update by rows, with new fft data
			textureOptions_t opts;
			opts.width			= N / 2;
			opts.height			= waterfallHeight;
			opts.dataType		= GL_R32F;
			opts.minFilter		= GL_NEAREST;
			opts.magFilter		= GL_NEAREST;
			opts.textureType	= GL_TEXTURE_2D;
			textureManager.Add( "Waterfall", opts );

			// load an audio file
			SDL_AudioSpec wavSpec;
			Uint32 wavLengthBytes;
			Uint8 *wavDataBuffer;

			// string filename = string( "../dennisMorrowMonoFloat.wav" );
			// string filename = string( "../cave14.wav" );
			// string filename = string( "../resultpele.wav" );
			string filename = string( "../groupB.wav" );

			if ( SDL_LoadWAV( filename.c_str(), &wavSpec, &wavDataBuffer, &wavLengthBytes ) == NULL ) {
				cout << "\nCould not open test.wav: " << SDL_GetError() << newline;
			} else {
				// Do stuff with the WAV data, and then...
				cout << "\nLoaded " << filename << ", " << wavLengthBytes << " bytes" << newline;
				cout << "Details:" << newline;
				cout << "\tSample Rate:\t\t" << wavSpec.freq << newline;
				cout << "\tEndianness:\t\t" << ( ( SDL_AUDIO_ISBIGENDIAN( wavSpec.format ) ) ? "big" : "little" ) << newline;
				cout << "\tSignedness:\t\t" << ( ( SDL_AUDIO_ISSIGNED( wavSpec.format ) ) ? "signed" : "unsigned" ) << newline;
				cout << "\tData Type:\t\t" << ( ( SDL_AUDIO_ISFLOAT( wavSpec.format ) ) ? "float" : "integer" ) << newline;
				cout << "\tBits Per Sample:\t" << ( int ) SDL_AUDIO_BITSIZE( wavSpec.format ) << newline;
				cout << "\tChannels:\t\t" << ( int ) wavSpec.channels << newline;

				// create the audio stream, to pull data from
				streamBufferAnalyze = SDL_NewAudioStream( AUDIO_F32, 1, 48000, AUDIO_F32, 1, 48000 );
				int rc = SDL_AudioStreamPut( streamBufferAnalyze, wavDataBuffer, wavLengthBytes );
				if ( rc == -1 ) {
					cout << "Failed to put samples in stream: " << SDL_GetError() << newline;
				}

				// set the playback to begin
				SDL_AudioDeviceID dev = SDL_OpenAudioDevice( NULL, 0, &wavSpec, NULL, 0 );
				SDL_QueueAudio( dev, wavDataBuffer, wavLengthBytes );
				SDL_PauseAudioDevice( dev, 0 );

				// free the data - is this needed?
				SDL_FreeWAV( wavDataBuffer );
			}
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

			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			glUniform1i( glGetUniformLocation( shader, "dataSize" ), N );

			// waterfall/spectrogram ring buffer
			glUniform1i( glGetUniformLocation( shader, "waterfallOffset" ), waterfallRowUpdate );
			textureManager.BindImageForShader( "Waterfall", "waterfall", shader, 2 );

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

		// // fill out the array with some shit
		// static rng noise = rng( -0.1f, 0.1f );
		// for ( int i = 0; i < N; i++ ) {

		// 	// inputData[ i ][ 0 ] = 0.0f;
		// 	// for ( int j = 0; j < 10; j++ ) {
		// 	// 	inputData[ i ][ 0 ] += 0.125f * ( sin( 0.1f * j * ( i + offset ) ) + cos( 0.3f * j * ( i + offset / 3.0f ) ) );
		// 	// }

		// 	inputData[ i ][ 0 ] = 0.125f * (
		// 		0.7f * sin( RangeRemap( sin( 0.01f * offset ), -1.0f, 1.0f, 0.1f, 0.5f ) * ( i + offset ) ) +
		// 		0.5f * sin( RangeRemap( sin( 0.044f * offset ), -1.0f, 1.0f, 0.6f, 0.8f ) * ( i + offset ) ) +
		// 		0.3f * sin( RangeRemap( sin( 0.0127f * offset ), -1.0f, 1.0f, 0.2f, 1.6f ) * ( i + offset / 18.0f ) ) +
		// 		0.1f * cos( 0.123f * ( i + offset / 3.0f ) ) ) +
		// 		noise();

		// 	// pull samples from the audio stream

		// 	inputData[ i ][ 1 ] = 0.0f;
		// }


		// get the data out of the audio stream


		static float data[ N ];
		int gotten = SDL_AudioStreamGet( streamBufferAnalyze, data, sizeof ( data ) );
		if ( gotten == -1 ) {
			cout << "Uhoh, failed to get converted data: " << SDL_GetError() << newline;
		}

		for ( int i = 0; i < N; i++ ) {
			inputData[ i ][ 0 ] = data[ i ];
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

		// update the next row in the spectrogram waterfall graph image, with the same data that was passed to the fftBuffer
		glBindTexture( GL_TEXTURE_2D, textureManager.Get( "Waterfall" ) );
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, waterfallRowUpdate, N / 2, 1, GL_RED, GL_FLOAT, ( GLvoid * ) &outputDataCastToSinglePrecision );

		waterfallRowUpdate--;
		if ( waterfallRowUpdate < 0 ) {
			waterfallRowUpdate = waterfallHeight - 1;
		}
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

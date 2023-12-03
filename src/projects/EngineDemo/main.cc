#include "../../engine/engine.h"

class engineDemo : public engineBase {	// example derived class
public:
	engineDemo () { Init(); OnInit(); PostInit(); }
	~engineDemo () { Quit(); }

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Dummy Draw" ] = computeShader( "./src/projects/EngineDemo/shaders/dummyDraw.cs.glsl" ).shaderHandle;

		// very interesting - can call stuff from the command line
		// std::system( "./scripts/build.sh" ); // <- this works as expected, to run the build script

			// imagemagick? standalone image processing stuff in another jbDE child app? there's a huge amount of potential here
				// could write temporary json file, call something that would parse it and apply operations to an image? could be cool
				// something like bin/imageProcess <json path>, and have that json specify source file, a list of ops, and destination path

			// Image_4F testImage;
			// testImage.Load( "test.png" );

			// // extract the image's bytes out
			// std::vector< unsigned char > bytes;
			// for ( uint y = 0; y < testImage.Height(); y++ ) {
			// 	for ( uint x = 0; x < testImage.Width(); x++ ) {

			// 		color_4F color = testImage.GetAtXY( x, y );
			// 		float sourceData[ 4 ];
			// 		sourceData[ 0 ] = color[ red ];
			// 		sourceData[ 1 ] = color[ green ];
			// 		sourceData[ 2 ] = color[ blue ];
			// 		sourceData[ 3 ] = color[ alpha ];

			// 		for ( int i = 0; i < 4; i++ ) {

						// probably mess with this again, but with a low chance for random bit flips
							// needs nan checking, etc

			// 			bytes.push_back( *(( uint8_t* ) &sourceData[ i ] + 0 ) );
			// 			bytes.push_back( *(( uint8_t* ) &sourceData[ i ] + 1 ) );
			// 			bytes.push_back( *(( uint8_t* ) &sourceData[ i ] + 2 ) );
			// 			bytes.push_back( *(( uint8_t* ) &sourceData[ i ] + 3 ) );
			// 		}
			// 	}
			// }

			// // shuffle the bytes
			// std::random_shuffle( bytes.begin() + 1500000, bytes.end() - 1500000 );

			// // put the bytes back into the array
			// for ( uint i = 0; i < bytes.size(); i += 4 ) {
			// 	uint8_t data[ 4 ] = { bytes[ i ], bytes[ i + 1 ], bytes[ i + 2 ], bytes[ i + 3 ] };
			// 	testImage.GetImageDataBasePtr()[ i / 4 ] = std::clamp( *( float* )&data[ 0 ], 0.0f, 1.0f );
			// }

			// // save the image back out
			// testImage.Save( "out.png" );


		// report platform specific sized long floats
			// ( quad must be at least as precise as double, double as precise as float - only guarantee in the spec )
			// cout << "float: " << sizeof( float ) << " double: " << sizeof( double ) << " quad: " << sizeof( long double ) << endl << endl;

		}

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
		// draw some shit
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Dummy Draw" ] );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "time" ), SDL_GetTicks() / 1600.0f );
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
			// textRenderer.Update( ImGui::GetIO().DeltaTime );
			for ( int i  = 0; i < 1; i++ ) {
				// add some random lines
				static rngi xPick = rngi( 0, textRenderer.numBinsWidth - 1 );
				static rngi yPick = rngi( 0, textRenderer.numBinsHeight - 1 );
				static rngi rPick = rngi( 22, 255 );
				static rngi gPick = rngi( 4, 128 );
				static rngi bPick = rngi( 0, 255 );
				static rngi wPick = rngi( 5, 100 );
				static rngi hPick = rngi( 0, 2 );
				static rngi lPick = rngi( 176, 223 );
				static rngi type = rngi( 0, 1 );
				static rngi wordPick = rngi( 0, colorWords.size() - 1 );

				ivec2 basePt = ivec2( xPick(), yPick() );
				if ( type() == 0 ) {
					textRenderer.layers[ type() ].DrawRectConstant( uvec2( basePt ), uvec2( basePt ) + uvec2( wPick(), 0 ), cChar( RED, ( unsigned char ) lPick() ) );
					textRenderer.layers[ type() ].DrawRectConstant( uvec2( basePt ), uvec2( basePt ) + uvec2( wPick(), hPick() * 4 ), cChar( WHITE, ( unsigned char ) lPick() ) );
					textRenderer.layers[ type() ].DrawRectConstant( uvec2( basePt ), uvec2( basePt ) + uvec2( hPick(), 0 ), cChar( ivec3( rPick(), gPick(), bPick() ), ( unsigned char ) lPick() ) );

					string word = colorWords[ wordPick() ];
					basePt = ivec2( xPick(), yPick() );
					textRenderer.layers[ 1 ].WriteString( uvec2( basePt ), uvec2( basePt ) + uvec2( word.size(), 1 ), word, ivec3( bPick() ) );
				} else {
					textRenderer.layers[ type() ].DrawRectConstant( uvec2( basePt ), uvec2( basePt ) + uvec2( 0, wPick() ), cChar( RED, ( unsigned char ) lPick() ) );
				}
			}

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
	engineDemo engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

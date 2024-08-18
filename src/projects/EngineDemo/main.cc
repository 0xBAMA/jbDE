#include "../../engine/engine.h"

class engineDemo final : public engineBase { // sample derived from base engine class
public:
	engineDemo () { Init(); OnInit(); PostInit(); }
	~engineDemo () { Quit(); }

	bool terminalActive = true;

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

			// =============================================================================================================

		// // messing with data moshing
		// 	Image_4F testImage;
		// 	testImage.Load( "test.png" );

		// 	rngi pick = rngi( 0, 45 );

		// 	for ( size_t i = 0; i < testImage.GetData()->size(); i++ ) {
		// 		switch ( pick() ) {
		// 			case 1:
		// 				testImage.GetData()->insert( testImage.GetData()->begin() + i, 1, 0 );
		// 				testImage.GetData()->insert( testImage.GetData()->begin() + i, 1, 0 );
		// 				testImage.GetData()->insert( testImage.GetData()->begin() + i, 1, 0 );
		// 				break;
		// 			case 2:
		// 				testImage.GetData()->erase( testImage.GetData()->begin() + i );
		// 				testImage.GetData()->erase( testImage.GetData()->begin() + i );
		// 				testImage.GetData()->erase( testImage.GetData()->begin() + i );
		// 				break;
		// 			case 3:
		// 				( *testImage.GetData() )[ i ] *= rand();
		// 				( *testImage.GetData() )[ i + 1 ] *= rand();
		// 				( *testImage.GetData() )[ i + 2 ] *= rand();
		// 				break;
		// 			default:
		// 				break;
		// 		}
		// 	}

		// 	testImage.GetData()->resize( testImage.Width() * testImage.Height() * 4 );
		// 	testImage.Save( "out.png" );

			// =============================================================================================================

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

			// put some initial data in the terminal history
			textRenderer.initTerminal();

		}

	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// new data into the input handler
		inputHandler.update();

		// if ( inputHandler.getState4( KEY_F2 ) == KEYSTATE_RISING )
			// terminalActive = true; // refinements tbd

		// pass any signals into the terminal - move this somewhere nicer, soon
		if ( terminalActive == true ) {

		// navigation through history
			// if ( inputHandler.getState( KEY_UP ) ) {
				// textRenderer.ts.cursorY++;
			// }
			// if ( inputHandler.getState( KEY_DOWN ) ) {
				// textRenderer.ts.cursorY--;
			// }

			const bool control = inputHandler.getState( KEY_LEFT_CTRL ) || inputHandler.getState( KEY_RIGHT_CTRL );
			const bool shift = inputHandler.getState( KEY_LEFT_SHIFT ) || inputHandler.getState( KEY_RIGHT_SHIFT );

		// navigation within line
			if ( inputHandler.getState4( KEY_LEFT ) == KEYSTATE_RISING ) {
				textRenderer.ts.cursorLeft( control );
			}
			if ( inputHandler.getState4( KEY_RIGHT ) == KEYSTATE_RISING ) {
				textRenderer.ts.cursorRight( control );
			}
			if ( inputHandler.getState4( KEY_HOME ) == KEYSTATE_RISING ) {
				textRenderer.ts.home();
			}
			if ( inputHandler.getState4( KEY_END ) == KEYSTATE_RISING ) {
				textRenderer.ts.end();
			}


		// control inputs
			if ( inputHandler.getState4( KEY_BACKSPACE ) == KEYSTATE_RISING ) {
				textRenderer.ts.backspace( control );
			}
			if ( inputHandler.getState4( KEY_ENTER ) == KEYSTATE_RISING ) {
				textRenderer.ts.enter();
			}
			if ( inputHandler.getState4( KEY_DELETE ) == KEYSTATE_RISING ) {
				textRenderer.ts.deleteKey();
			}
			if ( inputHandler.getState4( KEY_SPACE ) == KEYSTATE_RISING ) {
				textRenderer.ts.addChar( ' ' );
			}

		// char input
			if ( inputHandler.stateBuffer[ inputHandler.currentOffset ].alphasActive() ) { // is this check useful?
				string letterString = string( " abcdefghijklmnopqrstuvwxyz" );
				for ( int i = 1; i <= 26; i++ ) {
					if ( inputHandler.getState4( ( keyName_t ) i ) == KEYSTATE_RISING ) {
						textRenderer.ts.addChar( shift ? toupper( letterString[ i ] ) : letterString[ i ] );
					}
				}
			}

		// numbers
			string numberString = string( "0123456789" );
			string shiftedString = string( ")!@#$%^&*(" );
			for ( int i = 27; i < 37; i++ ) {
				if ( inputHandler.getState4( ( keyName_t ) i ) == KEYSTATE_RISING ) {
					textRenderer.ts.addChar( shift ? shiftedString[ i - 27 ] : numberString[ i - 27 ] );
				}
			}

		// punctuation and assorted other shit
			if ( inputHandler.getState4( KEY_PERIOD ) == KEYSTATE_RISING ) {
				textRenderer.ts.addChar( shift ? '>' : '.' );
			}
			if ( inputHandler.getState4( KEY_COMMA ) == KEYSTATE_RISING ) {
				textRenderer.ts.addChar( shift ? '<': ',' );
			}
			if ( inputHandler.getState4( KEY_SLASH ) == KEYSTATE_RISING ) {
				textRenderer.ts.addChar( shift ? '?': '/' );
			}
			if ( inputHandler.getState4( KEY_SEMICOLON ) == KEYSTATE_RISING ) {
				textRenderer.ts.addChar( shift ? ':': ';' );
			}
			if ( inputHandler.getState4( KEY_BACKSLASH ) == KEYSTATE_RISING ) {
				textRenderer.ts.addChar( shift ? '|' : '\\' );
			}
			if ( inputHandler.getState4( KEY_LEFT_BRACKET ) == KEYSTATE_RISING ) {
				textRenderer.ts.addChar( shift ? '{' : '[' );
			}
			if ( inputHandler.getState4( KEY_RIGHT_BRACKET ) == KEYSTATE_RISING ) {
				textRenderer.ts.addChar( shift ? '}' : ']' );
			}
			if ( inputHandler.getState4( KEY_APOSTROPHE ) == KEYSTATE_RISING ) {
				textRenderer.ts.addChar( shift ? '"' : '\'' );
			}
			if ( inputHandler.getState4( KEY_GRAVE ) == KEYSTATE_RISING ) {
				textRenderer.ts.addChar( shift ? '~' : '`' );
			}
			if ( inputHandler.getState4( KEY_MINUS ) == KEYSTATE_RISING ) {
				textRenderer.ts.addChar( shift ? '_' : '-' );
			}
			if ( inputHandler.getState4( KEY_EQUALS ) == KEYSTATE_RISING ) {
				textRenderer.ts.addChar( shift ? '+' : '=' );
			}
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
			textRenderer.Update( ImGui::GetIO().DeltaTime );

			// show terminal
			if ( terminalActive == true )
				textRenderer.drawTerminal();

			// put the result on the display
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

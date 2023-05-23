#include "../../engine/engine.h"

float RangeRemap ( float value, float inLow, float inHigh, float outLow, float outHigh ) {
	return outLow + ( value - inLow ) * ( outHigh - outLow ) / ( inHigh - inLow );
}

class engineDemo : public engineBase {	// example derived class
public:
	engineDemo () { Init(); OnInit(); PostInit(); }
	~engineDemo () { Quit(); }

	void OnInit () {
		ZoneScoped;
		{ Block Start( "Additional User Init" );
			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Dummy Draw" ] = computeShader( "./src/projects/engineDemo/shaders/dummyDraw.cs.glsl" ).shaderHandle;

			// cout << newline;
			// cout << "loaded " << glyphList.size() << " glyphs" << newline;
			// cout << "loaded " << paletteList.size() << " palettes" << newline;
			// cout << "loaded " << badWords.size() << " bad words" << newline;
			// cout << "loaded " << colorWords.size() << " color words" << newline;

			// hardcoded image dimensions
			Image_4U comparison( 107 * 15, 150 * 25 );

			for ( int r1 = 0; r1 < 15; r1++ ) {
				for ( int r2 = 0; r2 < 25; r2++ ) {
					// load the test image from disk
					Image_4U test( "test.png" );

					// remap the ranges to usable float parameters
					float k1 = RangeRemap( r1, 0.0f, 15.0f, -1.0f, 1.0f );
					float k2 = RangeRemap( r2, 0.0f, 25.0f, -1.0f, 1.0f );
					test.BarrelDistort( k1, k2 );

					// add to the output buffer
					uint32_t baseX = r1 * 107;
					uint32_t baseY = r2 * 150;
					for ( uint32_t y = 0; y < 150; y++ ) {
						for ( uint32_t x = 0; x < 107; x++ ) {
							comparison.SetAtXY( baseX + x, baseY + y, test.GetAtXY( x, y ) );
						}
					}
				}
			}
			comparison.Save( "rangeVisualizeNormalized.png" );
		}
	}

	void HandleCustomEvents () {
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );
		// application specific controls
	}

	void ImguiPass () {
		ZoneScoped;
		TonemapControlsWindow();

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
			textRenderer.Draw( textures[ "Display Texture" ] );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // show trident with current orientation
			scopedTimer Start( "Trident" );
			trident.Update( textures[ "Display Texture" ] );
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

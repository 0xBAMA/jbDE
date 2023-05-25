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

			// source data
			Image_4U testSrc( "test.png" );
			testSrc.Resize( 0.45f );

			// somewhere to put the outputs
			Image_4U samples( testSrc.Width() * 10, testSrc.Height() * 10 );

			for ( int y { 0 }; y < 10; y++ ) {
				for ( int x { 0 }; x < 10; x++ ) {
					// copy the scaled data, to operate on
					Image_4U thisIteration( testSrc.Width(), testSrc.Height(), testSrc.GetImageDataBasePtr() );

					// test grid showing parameter ranges
					const float strength = RangeRemap( x, 0.0f, 10.0f, 1.0f, 5.0f );
					const float cylinder = RangeRemap( y, 0.0f, 10.0f, 0.0f, 5.0f );
					thisIteration.DeCarpentierLensDistort( strength, cylinder );

					// put it into the samples image
					const uint32_t baseX = testSrc.Width() * x;
					const uint32_t baseY = testSrc.Height() * y;
					for ( uint32_t yy = 0; yy < thisIteration.Height(); yy++ ) {
						for ( uint32_t xx = 0; xx < thisIteration.Width(); xx++ ) {
							samples.SetAtXY( baseX + xx, baseY + yy, thisIteration.GetAtXY( xx, yy ) );
						}
					}
				}
			}
			samples.Save( "distorted.png" );
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

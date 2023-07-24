#include "../../../engine/engine.h"

struct sirenConfig_t {

	uint32_t targetWidth;
	uint32_t targetHeight;
	uint32_t performanceHistorySamples;

	uint32_t tileSize;
	uint32_t tilePerFrameCap;
	int32_t sampleCountCap;	// -1 for unlimited
	uvec2 tileOffset;
	uvec2 blueNoiseOffset;

	uint32_t raymarchMaxSteps;
	uint32_t raymarchMaxBounces;
	float raymarchMaxDistance;
	float raymarchEpsilon;

	float exposure;
	float renderFoV;
	vec3 viewerPosition;	// orientation will come from the trident

// questionable:
	// dither parameters ( mode, colorspace, pattern )
	// depth fog parameters ( mode, scalar )
	// display mode ( preview depth, normals, colors, pathtrace )
	// thin lens parameters ( focus distance, disk offset jitter )
	// normal mode - I think this doesn't really make sense to include, because only one really worked correctly last time

};

class Siren : public engineBase {	// example derived class
public:
	Siren () { Init(); OnInit(); PostInit(); }
	~Siren () { Quit(); }

	sirenConfig_t sirenConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			shaders[ "Dummy Draw" ] = computeShader( "./src/projects/PathTracing/Siren/shaders/dummyDraw.cs.glsl" ).shaderHandle;

			json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();
			sirenConfig.targetWidth					= j[ "app" ][ "Siren" ][ "targetWidth" ];
			sirenConfig.targetHeight				= j[ "app" ][ "Siren" ][ "targetHeight" ];
			sirenConfig.tileSize					= j[ "app" ][ "Siren" ][ "tileSize" ];
			sirenConfig.tilePerFrameCap				= j[ "app" ][ "Siren" ][ "tilePerFrameCap" ];
			sirenConfig.performanceHistorySamples	= j[ "app" ][ "Siren" ][ "performanceHistorySamples" ];
			sirenConfig.raymarchMaxSteps			= j[ "app" ][ "Siren" ][ "raymarchMaxSteps" ];
			sirenConfig.raymarchMaxBounces			= j[ "app" ][ "Siren" ][ "raymarchMaxBounces" ];
			sirenConfig.raymarchMaxDistance			= j[ "app" ][ "Siren" ][ "raymarchMaxDistance" ];
			sirenConfig.raymarchEpsilon				= j[ "app" ][ "Siren" ][ "raymarchEpsilon" ];
			sirenConfig.exposure					= j[ "app" ][ "Siren" ][ "exposure" ];
			sirenConfig.renderFoV					= j[ "app" ][ "Siren" ][ "renderFoV" ];
			sirenConfig.viewerPosition.x			= j[ "app" ][ "Siren" ][ "viewerPosition" ][ "x" ];
			sirenConfig.viewerPosition.y			= j[ "app" ][ "Siren" ][ "viewerPosition" ][ "y" ];
			sirenConfig.viewerPosition.z			= j[ "app" ][ "Siren" ][ "viewerPosition" ][ "z" ];

		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );
		// const uint8_t * state = SDL_GetKeyboardState( NULL );
	}

	void ImguiPass () {
		ZoneScoped;

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

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
			textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // show trident with current orientation
			scopedTimer Start( "Trident" );
			trident.Update( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
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
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	Siren sirenInstance;
	while( !sirenInstance.MainLoop() );
	return 0;
}

#include "../../../engine/engine.h"

struct sirenConfig_t {
// program parameters and state

	// performance settings / monitoring
	uint32_t performanceHistorySamples;
	std::deque< float > fpsHistory;
	std::deque< float > tileHistory;
	uint32_t numFullscreenPasses = 0;
	int32_t sampleCountCap;				// -1 for unlimited

	// renderer state
	uint32_t tileSize;
	uint32_t targetWidth;
	uint32_t targetHeight;
	uint32_t tilePerFrameCap;
	bool tileListNeedsUpdate = true;
	std::vector< ivec2 > tileOffsets;	// shuffled list of tiles
	uint32_t tileOffset;				// offset into tile list
	
	ivec2 blueNoiseOffset;
	float exposure;
	float renderFoV;
	vec3 viewerPosition;	// orientation will come from the trident, I think

	// raymarch parameters
	uint32_t raymarchMaxSteps;
	uint32_t raymarchMaxBounces;
	float raymarchMaxDistance;
	float raymarchEpsilon;
	float raymarchUnderstep;


// questionable need:
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
			// preview shader
			// pathtrace shader
			// custom tonemap shader, to use the 32-bit float accumulator
			// ...

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
			sirenConfig.raymarchUnderstep			= j[ "app" ][ "Siren" ][ "raymarchUnderstep" ];
			sirenConfig.exposure					= j[ "app" ][ "Siren" ][ "exposure" ];
			sirenConfig.renderFoV					= j[ "app" ][ "Siren" ][ "renderFoV" ];
			sirenConfig.viewerPosition.x			= j[ "app" ][ "Siren" ][ "viewerPosition" ][ "x" ];
			sirenConfig.viewerPosition.y			= j[ "app" ][ "Siren" ][ "viewerPosition" ][ "y" ];
			sirenConfig.viewerPosition.z			= j[ "app" ][ "Siren" ][ "viewerPosition" ][ "z" ];

			// remove the 16-bit accumulator, because we're going to want to use a 32-bit version
			textureManager.Remove( "Accumulator" );

			// create the new accumulator(s)
			textureOptions_t opts;
			opts.dataType		= GL_RGBA32F;
			opts.width			= sirenConfig.targetWidth;
			opts.height			= sirenConfig.targetHeight;
			opts.textureType	= GL_TEXTURE_2D;
			textureManager.Add( "Color Accumulator", opts );
			textureManager.Add( "Depth/Normals Accumulator", opts );

			// initial blue noise offset
			UpdateNoiseOffset();
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

	void ResetAccumulators () {
		// clear the buffers

	}

	void updateNoiseOffset () {
		static rng offsetGenerator = rng( 0, 512 );
		sirenConfig.blueNoiseOffset = ivec2( offsetGenerator(), offsetGenerator() );
	}

	ivec2 GetTile () {
		if ( sirenConfig.tileListNeedsUpdate == true ) {
			// construct the tile list ( runs at frame 0 and again any time the tilesize changes )
			sirenConfig.tileListNeedsUpdate = false;
			for ( int x = 0; x <= config.width; x += sirenConfig.tileSize ) {
				for ( int y = 0; y <= config.height; y += sirenConfig.tileSize ) {
					sirenConfig.tileOffsets.push_back( ivec2( x, y ) );
				}
			}
		} else { // check if the offset needs to be reset, this means a full pass has been completed
			if ( ++sirenConfig.tileOffset == sirenConfig.tileOffsets.size() ) {
				sirenConfig.tileOffset = 0;
				sirenConfig.numFullscreenPasses++;
			}
		}
		// shuffle when listOffset is zero ( first iteration, and any subsequent resets )
		if ( !sirenConfig.tileOffset ) {
			std::random_device rd;
			std::mt19937 rngen( rd() );
			std::shuffle( sirenConfig.tileOffsets.begin(), sirenConfig.tileOffsets.end(), rngen );
		}
		return sirenConfig.tileOffsets[ sirenConfig.tileOffset ];
	}

	void ComputePasses () {
		ZoneScoped;

		{
			scopedTimer Start( "Tiled Update" );
			glUseProgram( shaders[ "Pathtrace" ] );



			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // postprocessing - shader for color grading ( color temp, contrast, gamma ... ) + tonemapping
			scopedTimer Start( "Postprocess" );
			glUseProgram( shaders[ "Tonemap" ] );

			// bind appropriate images

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

		{ // show trident with current orientation
			scopedTimer Start( "Trident" );
			trident.Update( textureManager.Get( "Display Texture" ) );
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
	Siren sirenInstance;
	while( !sirenInstance.MainLoop() );
	return 0;
}

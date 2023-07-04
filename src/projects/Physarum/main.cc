#include "../../engine/engine.h"

struct physarumConfig_t {
	uint32_t numAgents;
	float senseAngle;
	float senseDistance;
	float turnAngle;
	float stepSize;
	float decayFactor;
	uint32_t depositAmount;
};

class Physarum : public engineBase {
public:
	Physarum () { Init(); OnInit(); PostInit(); }
	~Physarum () { Quit(); }

	physarumConfig_t pysarumConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture - comes from the demo project
			const string basePath = "./src/projects/Physarum/shaders/";
			shaders[ "Dummy Draw" ]			= computeShader( basePath + "dummyDraw.cs.glsl" ).shaderHandle;
			shaders[ "Diffuse and Decay" ]	= computeShader( basePath + "diffuseAndDecay.cs.glsl" ).shaderHandle;
			shaders[ "Agents" ]				= regularShader( basePath + "agent.vs.glsl", basePath + "agent.fs.glsl" ).shaderHandle;
			shaders[ "Continuum" ]			= regularShader( basePath + "continuum.vs.glsl", basePath + "continuum.fs.glsl" ).shaderHandle;

			// get the configuration from config.json
			json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();
			pysarumConfig.numAgents		= j[ "app" ][ "Physarum" ][ "numAgents" ];
			pysarumConfig.senseAngle	= j[ "app" ][ "Physarum" ][ "senseAngle" ];
			pysarumConfig.senseDistance	= j[ "app" ][ "Physarum" ][ "senseDistance" ];
			pysarumConfig.turnAngle		= j[ "app" ][ "Physarum" ][ "turnAngle" ];
			pysarumConfig.stepSize		= j[ "app" ][ "Physarum" ][ "stepSize" ];
			pysarumConfig.decayFactor	= j[ "app" ][ "Physarum" ][ "decayFactor" ];
			pysarumConfig.depositAmount	= j[ "app" ][ "Physarum" ][ "depositAmount" ];

			// setup the ssbo for the agent data
			struct agent_t {
				vec2 position;
				vec2 direction;
			};

			std::vector< agent_t > agentsInitialData;

			// setup the image buffers for the atomic writes ( 2x for ping-ponging )

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

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );
			textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		// { // show trident with current orientation - has no relevance for this project
		// 	scopedTimer Start( "Trident" );
		// 	trident.Update( textureManager.Get( "Display Texture" ) );
		// 	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		// }
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );

		// run the shader to move the points

		// run the shader to do the gaussian blur

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
	Physarum engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

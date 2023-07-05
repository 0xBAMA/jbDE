#include "../../engine/engine.h"

struct physarumConfig_t {
	// environment setup
	uint32_t numAgents;
	uint32_t dimensionX;
	uint32_t dimensionY;

	// agent sim
	float senseAngle;
	float senseDistance;
	float turnAngle;
	float stepSize;

	// diffuse + decay
	float decayFactor;
	uint32_t depositAmount;
};

class Physarum : public engineBase {
public:
	Physarum () { Init(); OnInit(); PostInit(); }
	~Physarum () { Quit(); }

	physarumConfig_t physarumConfig;

	GLuint agentSSBO;

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
			physarumConfig.numAgents		= j[ "app" ][ "Physarum" ][ "numAgents" ];
			physarumConfig.dimensionX		= j[ "app" ][ "Physarum" ][ "dimensionX" ];
			physarumConfig.dimensionY		= j[ "app" ][ "Physarum" ][ "dimensionY" ];
			physarumConfig.senseAngle		= j[ "app" ][ "Physarum" ][ "senseAngle" ];
			physarumConfig.senseDistance	= j[ "app" ][ "Physarum" ][ "senseDistance" ];
			physarumConfig.turnAngle		= j[ "app" ][ "Physarum" ][ "turnAngle" ];
			physarumConfig.stepSize			= j[ "app" ][ "Physarum" ][ "stepSize" ];
			physarumConfig.decayFactor		= j[ "app" ][ "Physarum" ][ "decayFactor" ];
			physarumConfig.depositAmount	= j[ "app" ][ "Physarum" ][ "depositAmount" ];

			// setup the ssbo for the agent data
			struct agent_t {
				vec2 position;
				vec2 direction;
			};

			std::vector< agent_t > agentsInitialData;
			size_t bufferSize = 4 * sizeof( GLfloat ) * physarumConfig.numAgents;
			rng dist( -1.0f, 1.0f );

			for ( uint32_t i = 0; i < physarumConfig.numAgents; i++ ) {
				agentsInitialData.push_back( { { dist(), dist() }, glm::normalize( { dist(), dist() } ) } );
			}

			glGenBuffers( 1, &agentSSBO );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, agentSSBO );
			glBufferData( GL_SHADER_STORAGE_BUFFER, bufferSize, ( GLvoid * ) &agentsInitialData[ 0 ], GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, agentSSBO );

			// setup the image buffers for the atomic writes ( 2x for ping-ponging )
			textureOptions_t opts;
			opts.dataType		= GL_R32UI;
			opts.width			= physarumConfig.dimensionX;
			opts.height			= physarumConfig.dimensionY;
			opts.textureType	= GL_TEXTURE_2D;
			textureManager.Add( "Pheremone Continuum Buffer 0", opts );
			textureManager.Add( "Pheremone Continuum Buffer 1", opts );
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

		// // run the shader to do the gaussian blur
		// glUseProgram( diffuse_and_decay_shader );

		// //swap the images
		// std::swap( continuum_textures[ 0 ], continuum_textures[ 1 ] );
		// glBindImageTexture( 1, continuum_textures[ 0 ], 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI );
		// glBindImageTexture( 2, continuum_textures[ 1 ], 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI );
		// glUniform1f( glGetUniformLocation( diffuse_and_decay_shader, "decay_factor" ), decay_factor );

		// glDispatchCompute( DIM / 8, DIM / 8, 1 );
		// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

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

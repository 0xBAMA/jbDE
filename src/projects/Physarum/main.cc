#include "../../engine/engine.h"

struct physarumConfig_t {
	// environment setup
	uint32_t numAgents;
	uint32_t dimensionX;
	uint32_t dimensionY;
	bool showTrails;
	bool showAgents;
	bool oddFrame = false;

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
	GLuint continuumVAO;
	GLuint continuumVBO;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture - comes from the demo project
			const string basePath = "./src/projects/Physarum/shaders/";
			shaders[ "Buffer Copy" ]		= computeShader( basePath + "bufferCopy.cs.glsl" ).shaderHandle;
			shaders[ "Diffuse and Decay" ]	= computeShader( basePath + "diffuseAndDecay.cs.glsl" ).shaderHandle;

			// pending refactor
			shaders[ "Agents" ]				= computeShader( basePath + "agent.cs.glsl" ).shaderHandle;
			// shaders[ "Agents" ]				= regularShader( basePath + "agent.vs.glsl", basePath + "agent.fs.glsl" ).shaderHandle;

			// get the configuration from config.json
			json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();
			physarumConfig.numAgents		= j[ "app" ][ "Physarum" ][ "numAgents" ];
			physarumConfig.dimensionX		= j[ "app" ][ "Physarum" ][ "dimensionX" ];
			physarumConfig.dimensionY		= j[ "app" ][ "Physarum" ][ "dimensionY" ];
			physarumConfig.showTrails		= j[ "app" ][ "Physarum" ][ "showTrails" ];
			physarumConfig.showAgents		= j[ "app" ][ "Physarum" ][ "showAgents" ];
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
				agentsInitialData.push_back( { { dist(), dist() }, glm::normalize( vec2( dist(), dist() ) ) } );
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
		if ( tonemap.showTonemapWindow ) {
			TonemapControlsWindow();
		}

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		// expose controls for the physarum
		if ( ImGui::SmallButton( "Randomize Parameters" ) ) {
			long unsigned int seed = std::chrono::system_clock::now().time_since_epoch().count();
			std::default_random_engine engine{ seed };
			std::uniform_real_distribution<GLfloat> senseAngleDistribution( 0.0f, float( pi ) );
			std::uniform_real_distribution<GLfloat> senseDistanceDistribution( 0.0f, 0.005f );
			std::uniform_real_distribution<GLfloat> turnAngleDistribution( 0.0f, float( pi ) );
			std::uniform_real_distribution<GLfloat> stepSizeDistribution( 0.0f, 0.005f );
			std::uniform_int_distribution<int> depositAmountDistribution( 4000, 75000 );
			std::uniform_real_distribution<GLfloat> decayFactorDistribution( 0.75f, 1.0f );

			// generate new values based on above distributions
			physarumConfig.senseAngle		= senseAngleDistribution( engine );
			physarumConfig.senseDistance	= senseDistanceDistribution( engine );
			physarumConfig.turnAngle		= turnAngleDistribution( engine );
			physarumConfig.stepSize			= stepSizeDistribution( engine );
			physarumConfig.depositAmount	= depositAmountDistribution( engine );
			physarumConfig.decayFactor		= decayFactorDistribution( engine );
		}

		// widgets
		ImGui::Text("Sensor Angle:                ");
		ImGui::SameLine();
		HelpMarker("The angle between the sensors.");
		ImGui::SliderFloat("radians", &physarumConfig.senseAngle, 0.0f, 3.14f, "%.4f");

		ImGui::Separator();

		ImGui::Text("Sensor Distance:             ");
		ImGui::SameLine();
		HelpMarker("The distance from the agent position to the sensors.");
		ImGui::SliderFloat("        ", &physarumConfig.senseDistance, 0.0f, 0.005f, "%.4f");

		ImGui::Separator();

		ImGui::Text("Turn Angle:                  ");
		ImGui::SameLine();
		HelpMarker("Amount that each simulation agent can turn in the movement shader.");
		ImGui::SliderFloat("radians ", &physarumConfig.turnAngle, 0.0f, 3.14f, "%.4f");

		ImGui::Separator();

		ImGui::Text("Step Size:                   ");
		ImGui::SameLine();
		HelpMarker("Distance that each sim agent will go in their current direction each step.");
		ImGui::SliderFloat("    ", &physarumConfig.stepSize, 0.0f, 0.005f, "%.4f");

		ImGui::Separator();

		ImGui::Text("Deposit Amount:              ");
		ImGui::SameLine();
		HelpMarker("Amout of pheremone that is deposited by each simulation agent.");
		ImGui::DragScalar("  ", ImGuiDataType_U32, &physarumConfig.depositAmount, 50, NULL, NULL, "%u units");

		ImGui::Separator();

		ImGui::Text("Decay Factor:                ");
		ImGui::SameLine();
		HelpMarker("Scale factor applied when storing the result of the gaussian blur.");
		ImGui::SliderFloat("              ", &physarumConfig.decayFactor, 0.75f, 1.0f, "%.4f");

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void DrawAPIGeometry () {
		ZoneScoped; scopedTimer Start( "API Geometry" );

		// agents
		glUseProgram( shaders[ "Agents" ] );

		// generation of the random values to be used in the shader
		std::vector< glm::vec2 > randomDirections;
		long unsigned int seed = std::chrono::system_clock::now().time_since_epoch().count();

		std::default_random_engine gen{ seed };
		std::uniform_real_distribution< GLfloat > dist{ -1.0f, 1.0f };

		for ( int i = 0; i < 8; i++ )
			randomDirections.push_back( glm::normalize( vec2( dist( gen ), dist( gen ) ) ) );

		glUniform2fv( glGetUniformLocation( shaders[ "Agents" ], "randomValues" ), 8, glm::value_ptr( randomDirections[ 0 ] ) );

		textureManager.BindTexForShader( string( "Pheremone Continuum Buffer " ) + string( physarumConfig.oddFrame ? "0" : "1" ), "current", shaders[ "Agents" ], 2 );

		// the rest of the simulation parameters
		glUniform1f( glGetUniformLocation( shaders[ "Agents" ], "stepSize" ), physarumConfig.stepSize );
		glUniform1f( glGetUniformLocation( shaders[ "Agents" ], "senseAngle" ), physarumConfig.senseAngle );
		glUniform1f( glGetUniformLocation( shaders[ "Agents" ], "senseDistance" ), physarumConfig.senseDistance );
		glUniform1f( glGetUniformLocation( shaders[ "Agents" ], "turnAngle" ), physarumConfig.turnAngle );
		glUniform1ui( glGetUniformLocation( shaders[ "Agents" ], "depositAmount" ), physarumConfig.depositAmount );
		glUniform1ui( glGetUniformLocation( shaders[ "Agents" ], "numAgents" ), physarumConfig.numAgents );

		// rounded up
		const int numSlices = ( physarumConfig.numAgents + 1023 ) / 1024;

		glDispatchCompute( 1, numSlices, 1 );
		glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();

			glUseProgram( shaders[ "Buffer Copy" ] );
			glUniform1i( glGetUniformLocation( shaders[ "Buffer Copy" ], "show_trails" ), physarumConfig.showTrails );
			glUniform2f( glGetUniformLocation( shaders[ "Buffer Copy" ], "resolution" ), config.width, config.height );
			textureManager.BindTexForShader( string( "Pheremone Continuum Buffer " ) + string( physarumConfig.oddFrame ? "1" : "0" ), "continuum", shaders[ "Buffer Copy" ], 2 );

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

	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );

		// run the shader to do the gaussian blur
		glUseProgram( shaders[ "Diffuse and Decay" ] );

		//swap the images
		glBindImageTexture( 1, textureManager.Get( string( "Pheremone Continuum Buffer " ) + string( physarumConfig.oddFrame ? "0" : "1" ) ), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI ); // previous
		glBindImageTexture( 2, textureManager.Get( string( "Pheremone Continuum Buffer " ) + string( physarumConfig.oddFrame ? "1" : "0" ) ), 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI ); // current
		glUniform1f( glGetUniformLocation( shaders[ "Diffuse and Decay" ], "decayFactor" ), physarumConfig.decayFactor );
		physarumConfig.oddFrame = !physarumConfig.oddFrame;

		glDispatchCompute( ( physarumConfig.dimensionX + 7 ) / 8, ( physarumConfig.dimensionY + 7 ) / 8, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

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

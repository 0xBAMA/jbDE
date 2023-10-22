#include "../../../engine/engine.h"

struct preset_t {
	float senseAngle;
	float senseDistance;
	float turnAngle;
	float stepSize;
	float decayFactor;
	uint32_t depositAmount;
	bool writeBack;
};

static std::vector< preset_t > presets;

struct physarumConfig_t {
	// environment setup
	uint32_t numAgents;
	uint32_t dimensionX;
	uint32_t dimensionY;
	uint32_t dimensionZ;
	bool oddFrame = false;

	// agent sim
	float senseAngle;
	float senseDistance;
	float turnAngle;
	float stepSize;
	bool writeBack;
	rngi wangSeed = rngi( 0, 100000 );

	// diffuse + decay
	float decayFactor;
	uint32_t depositAmount;

	vec3 color;
	float brightness;
	float zOffset = 0.0f;
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
			const string basePath = "./src/projects/Physarum/3D/shaders/";
			shaders[ "Buffer Copy" ]		= computeShader( basePath + "bufferCopy.cs.glsl" ).shaderHandle;
			shaders[ "Lighting" ]			= computeShader( basePath + "lighting.cs.glsl" ).shaderHandle;
			shaders[ "Diffuse and Decay" ]	= computeShader( basePath + "diffuseAndDecay.cs.glsl" ).shaderHandle;
			shaders[ "Agents" ]				= computeShader( basePath + "agent.cs.glsl" ).shaderHandle;

			// get the configuration from config.json
			json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();
			physarumConfig.numAgents		= j[ "app" ][ "Physarum3D" ][ "numAgents" ];
			physarumConfig.dimensionX		= j[ "app" ][ "Physarum3D" ][ "dimensionX" ];
			physarumConfig.dimensionY		= j[ "app" ][ "Physarum3D" ][ "dimensionY" ];
			physarumConfig.dimensionZ		= j[ "app" ][ "Physarum3D" ][ "dimensionZ" ];
			physarumConfig.brightness		= j[ "app" ][ "Physarum3D" ][ "brightness" ];
			physarumConfig.color			= vec3(
				j[ "app" ][ "Physarum3D" ][ "color" ][ "r" ],
				j[ "app" ][ "Physarum3D" ][ "color" ][ "g" ],
				j[ "app" ][ "Physarum3D" ][ "color" ][ "b" ]
			);

			// populate the presets vector
			json j2 = j[ "app" ][ "PhysarumPresets" ];
			for ( auto& data : j2 ) {
				preset_t preset;
				preset.senseAngle		= data[ "senseAngle" ];
				preset.senseDistance	= data[ "senseDistance" ];
				preset.turnAngle		= data[ "turnAngle" ];
				preset.stepSize			= data[ "stepSize" ];
				preset.writeBack		= data[ "writeBack" ];
				preset.decayFactor		= data[ "decayFactor" ];
				preset.depositAmount	= data[ "depositAmount" ];
				presets.push_back( preset );
			}

			ApplyPreset( 0 );

			// setup the ssbo for the agent data
			struct agent_t {
				vec4 position;
				vec4 basisX;
				vec4 basisY;
				vec4 basisZ;
				// other info? parameters per-agent?
			};

			std::vector< agent_t > agentsInitialData;
			size_t bufferSize = 16 * sizeof( GLfloat ) * physarumConfig.numAgents;

			// rng dist( -1.0f, 1.0f );
			rngN dist( 0.0f, 0.1f );
			rng dist2( -pi, pi );

			// init the progress bar
			progressBar bar;
			bar.total = physarumConfig.numAgents;
			bar.label = string( " Generating Initial Data: " );

			Tick();
			cout << endl;
			for ( uint32_t i = 0; i < physarumConfig.numAgents; i++ ) {
				orientTrident t; // randomize the orientation
				for ( int j = 0; j < 3; j++ ) {
					t.RotateX( dist2() );
					t.RotateY( dist2() );
					t.RotateZ( dist2() );
				}

				agentsInitialData.push_back( {
					{ dist(), dist(), dist2(), dist() },
					{ t.basisX, dist() },
					{ t.basisY, dist() },
					{ t.basisZ, dist() }
				} );

				// update and report
				bar.done = i;
				if ( i % 50 == 0 ) {
					bar.writeCurrentState();
				}
			}
			cout << endl;

			glGenBuffers( 1, &agentSSBO );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, agentSSBO );
			glBufferData( GL_SHADER_STORAGE_BUFFER, bufferSize, ( GLvoid * ) &agentsInitialData[ 0 ], GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, agentSSBO );

			// setup the image buffers for the atomic writes ( 2x for ping-ponging )
			textureOptions_t opts;
			opts.dataType		= GL_R32UI;
			opts.width			= physarumConfig.dimensionX;
			opts.height			= physarumConfig.dimensionY;
			opts.depth			= physarumConfig.dimensionZ;
			opts.textureType	= GL_TEXTURE_3D;
			textureManager.Add( "Pheremone Continuum Buffer 0", opts );
			textureManager.Add( "Pheremone Continuum Buffer 1", opts );

			// caches the lit color value, to be sampled during rendering
			opts.dataType		= GL_RGBA16F;
			textureManager.Add( "Shaded Volume", opts );
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
			rng senseAngle( 0.0f, float( pi ) );
			rng senseDistance( 0.0f, 0.005f );
			rng turnAngle( 0.0f, float( pi ) );
			rng stepSize( 0.0f, 0.005f );
			rngi depositAmount( 4000, 75000 );
			rng decayFactor( 0.75f, 1.0f );

			// generate new values based on above distributions
			physarumConfig.senseAngle		= senseAngle();
			physarumConfig.senseDistance	= senseDistance();
			physarumConfig.turnAngle		= turnAngle();
			physarumConfig.stepSize			= stepSize();
			physarumConfig.depositAmount	= depositAmount();
			physarumConfig.decayFactor		= decayFactor();
		}

		// widgets
		HelpMarker( "The angle between the sensors." );
		ImGui::SameLine();
		ImGui::Text( "Sensor Angle:" );
		ImGui::SliderFloat( "radians", &physarumConfig.senseAngle, 0.0f, 3.14f, "%.4f" );

		ImGui::Separator();

		HelpMarker( "The distance from the agent position to the sensors." );
		ImGui::SameLine();
		ImGui::Text( "Sensor Distance:" );
		ImGui::SliderFloat( "        ", &physarumConfig.senseDistance, 0.0f, 0.005f, "%.4f" );

		ImGui::Separator();

		HelpMarker( "Amount that each simulation agent can turn in the movement shader." );
		ImGui::SameLine();
		ImGui::Text( "Turn Angle:" );
		ImGui::SliderFloat( "radians ", &physarumConfig.turnAngle, 0.0f, 3.14f, "%.4f" );

		ImGui::Separator();

		HelpMarker( "Distance that each sim agent will go in their current direction each step." );
		ImGui::SameLine();
		ImGui::Text( "Step Size:" );
		ImGui::SliderFloat( "    ", &physarumConfig.stepSize, 0.0f, 0.005f, "%.4f" );

		ImGui::Separator();

		HelpMarker( "Amout of pheremone that is deposited by each simulation agent." );
		ImGui::SameLine();
		ImGui::Text( "Deposit Amount:" );
		ImGui::DragScalar( "  ", ImGuiDataType_U32, &physarumConfig.depositAmount, 50, NULL, NULL, "%u units" );

		ImGui::Separator();

		HelpMarker( "Scale factor applied when storing the result of the gaussian blur." );
		ImGui::SameLine();
		ImGui::Text( "Decay Factor:" );
		ImGui::SliderFloat( "              ", &physarumConfig.decayFactor, 0.75f, 1.0f, "%.4f" );

		ImGui::Checkbox( "Agent Direction Writeback", &physarumConfig.writeBack );

		ImGui::ColorEdit3( "Color", ( float * ) &physarumConfig.color, ImGuiColorEditFlags_PickerHueWheel );
		ImGui::SliderFloat( "Brightness", &physarumConfig.brightness, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic );

		ImGui::Separator();
		ImGui::Separator();
		ImGui::SliderFloat( "Display Z Offset", &physarumConfig.zOffset, 0.0f, 1.0f, "%.3f" );

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void DrawAPIGeometry () {
		ZoneScoped; scopedTimer Start( "API Geometry" );

	}

	void ComputePasses () {
		ZoneScoped;

		{ // update agents, held in the SSBO
			scopedTimer Start( "Agent Sim" );

			// send the simulation parameters
			glUseProgram( shaders[ "Agents" ] );
			glUniform1f( glGetUniformLocation( shaders[ "Agents" ], "stepSize" ), physarumConfig.stepSize );
			glUniform1f( glGetUniformLocation( shaders[ "Agents" ], "senseAngle" ), physarumConfig.senseAngle );
			glUniform1f( glGetUniformLocation( shaders[ "Agents" ], "senseDistance" ), physarumConfig.senseDistance );
			glUniform1f( glGetUniformLocation( shaders[ "Agents" ], "turnAngle" ), physarumConfig.turnAngle );
			glUniform1i( glGetUniformLocation( shaders[ "Agents" ], "writeBack" ), physarumConfig.writeBack );
			glUniform1i( glGetUniformLocation( shaders[ "Agents" ], "wangSeed" ), physarumConfig.wangSeed() );
			glUniform1ui( glGetUniformLocation( shaders[ "Agents" ], "depositAmount" ), physarumConfig.depositAmount );
			glUniform1ui( glGetUniformLocation( shaders[ "Agents" ], "numAgents" ), physarumConfig.numAgents );

			textureManager.BindTexForShader( string( "Pheremone Continuum Buffer " ) + string( physarumConfig.oddFrame ? "0" : "1" ), "current", shaders[ "Agents" ], 2 );

			// number of 1024-size jobs, rounded up
			const int numSlices = ( physarumConfig.numAgents + 1023 ) / 1024;

			glDispatchCompute( 1, numSlices, 1 );
			glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );
		}

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();

			glUseProgram( shaders[ "Lighting" ] );
			textureManager.BindTexForShader( string( "Pheremone Continuum Buffer " ) + string( physarumConfig.oddFrame ? "1" : "0" ), "continuum", shaders[ "Lighting" ], 2 );
			textureManager.BindImageForShader( "Shaded Volume", "shadedVolume", shaders[ "Lighting" ], 3 );
			glUniform3f( glGetUniformLocation( shaders[ "Lighting" ], "color" ), physarumConfig.color.r, physarumConfig.color.g, physarumConfig.color.b );
			glUniform1f( glGetUniformLocation( shaders[ "Lighting" ], "brightness" ), physarumConfig.brightness );
			glUniform1f( glGetUniformLocation( shaders[ "Lighting" ], "zOffset" ), physarumConfig.zOffset );

			static vec3 lightDirection = glm::normalize( vec3( 1.0f ) );
			const float amt = 0.001f;
			const float amt2 = 0.00451f;
			const float amt3 = 0.001618f;
			lightDirection = vec3( mat2( cos( amt ), -sin( amt ), sin( amt ), cos( amt ) ) * lightDirection.xy(), lightDirection.z );
			lightDirection = vec3( lightDirection.x, mat2( cos( amt2 ), -sin( amt2 ), sin( amt2 ), cos( amt2 ) ) * lightDirection.yz() );
			lightDirection = vec3( mat2( cos( amt3 ), -sin( amt3 ), sin( amt3 ), cos( amt3 ) ) * lightDirection.xy(), lightDirection.z );
			glUniform3f( glGetUniformLocation( shaders[ "Lighting" ], "lightDirection" ), lightDirection.x, lightDirection.y, lightDirection.z );

			glDispatchCompute( ( physarumConfig.dimensionX + 7 ) / 8, ( physarumConfig.dimensionY + 7 ) / 8, ( physarumConfig.dimensionZ + 7 ) / 8 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

			glUseProgram( shaders[ "Buffer Copy" ] );
			glUniform3f( glGetUniformLocation( shaders[ "Buffer Copy" ], "color" ), physarumConfig.color.r, physarumConfig.color.g, physarumConfig.color.b );
			glUniform1f( glGetUniformLocation( shaders[ "Buffer Copy" ], "brightness" ), physarumConfig.brightness );
			glUniform2f( glGetUniformLocation( shaders[ "Buffer Copy" ], "resolution" ), config.width, config.height );
			glUniform1f( glGetUniformLocation( shaders[ "Buffer Copy" ], "zOffset" ), physarumConfig.zOffset );
			textureManager.BindTexForShader( "Shaded Volume", "shadedVolume", shaders[ "Buffer Copy" ], 3 );
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

	void ApplyPreset( int idx ) {
		physarumConfig.senseAngle		= presets[ idx ].senseAngle;
		physarumConfig.senseDistance	= presets[ idx ].senseDistance;
		physarumConfig.turnAngle		= presets[ idx ].turnAngle;
		physarumConfig.stepSize			= presets[ idx ].stepSize;
		physarumConfig.decayFactor		= presets[ idx ].decayFactor;
		physarumConfig.depositAmount	= presets[ idx ].depositAmount;
		physarumConfig.writeBack		= presets[ idx ].writeBack;
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

		glDispatchCompute( ( physarumConfig.dimensionX + 7 ) / 8, ( physarumConfig.dimensionY + 7 ) / 8, ( physarumConfig.dimensionZ + 7 ) / 8 );
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

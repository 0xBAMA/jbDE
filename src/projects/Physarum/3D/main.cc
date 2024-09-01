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
	bool runSim = true;
	float senseAngle;
	float senseDistance;
	float turnAngle;
	float stepSize;
	bool writeBack;
	rngi wangSeed = rngi( 0, 100000 );

	// diffuse + decay
	float decayFactor;
	uint32_t depositAmount;

	// viewer position and orientation
	vec3 viewerPosition = vec3( 0.0f );
	vec3 viewerBasisX = vec3( 1.0f, 0.0f, 0.0f );
	vec3 viewerBasisY = vec3( 0.0f, 1.0f, 0.0f );
	vec3 viewerBasisZ = vec3( 0.0f, 0.0f, 1.0f );
	float viewerFoV = 0.618f;

	// scattering density threshold
	int densityThreshold = 5000;
};

class Physarum final : public engineBase {
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
			shaders[ "Draw" ]				= computeShader( basePath + "draw.cs.glsl" ).shaderHandle;
			shaders[ "Diffuse and Decay" ]	= computeShader( basePath + "diffuseAndDecay.cs.glsl" ).shaderHandle;
			shaders[ "Agents" ]				= computeShader( basePath + "agent.cs.glsl" ).shaderHandle;

			glObjectLabel( GL_PROGRAM, shaders[ "Draw" ], -1, string( "Draw" ).c_str() );
			glObjectLabel( GL_PROGRAM, shaders[ "Diffuse and Decay" ], -1, string( "Diffuse and Decay" ).c_str() );
			glObjectLabel( GL_PROGRAM, shaders[ "Agents" ], -1, string( "Agents" ).c_str() );

			// get the configuration from config.json
			json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();
			physarumConfig.numAgents		= j[ "app" ][ "Physarum3D" ][ "numAgents" ];
			physarumConfig.dimensionX		= j[ "app" ][ "Physarum3D" ][ "dimensionX" ];
			physarumConfig.dimensionY		= j[ "app" ][ "Physarum3D" ][ "dimensionY" ];
			physarumConfig.dimensionZ		= j[ "app" ][ "Physarum3D" ][ "dimensionZ" ];

			// populate the presets vector
			json j2; ifstream i2 ( "src/projects/Physarum/presets.json" ); i2 >> j2; i2.close();
			for ( auto& data : j2[ "PhysarumPresets" ] ) {
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

			rngN dist( 0.0f, 0.1f );
			rng dist2( -pi, pi );
			rng dist3( 0.0f, 1.0f );

			// init the progress bar
			progressBar bar;
			bar.total = physarumConfig.numAgents;
			bar.label = string( " Generating Initial Data: " );

			Tick();
			cout << endl;
			for ( uint32_t i = 0; i < physarumConfig.numAgents; i++ ) {
				orientTrident t; // randomize the orientation
				t.RotateX( dist2() );
				t.RotateY( dist2() );
				t.RotateZ( dist2() );

				agentsInitialData.push_back( {
					{ dist(), dist(), dist(), dist() },
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

			terminal.addCommand(
				{ "viewState" },
				{},
				[=] ( args_t args ) {

					// FoV:      xxx
					// position: xxx yyy zzz
					// basisX:   xxx yyy zzz
					// basisY:   xxx yyy zzz
					// basisZ:   xxx yyy zzz

					terminal.addHistoryLine( terminal.csb.append( "FoV:      " + fixedWidthNumberStringF( physarumConfig.viewerFoV, 5 ) ).flush() );
					terminal.addHistoryLine( terminal.csb.append( "Position: " + fixedWidthNumberStringF( physarumConfig.viewerPosition.x, 5 ) + " " + fixedWidthNumberStringF( physarumConfig.viewerPosition.y, 5 ) + " " + fixedWidthNumberStringF( physarumConfig.viewerPosition.z, 5 ) ).flush() );
					terminal.addHistoryLine( terminal.csb.append( "Basis X:  " + fixedWidthNumberStringF( physarumConfig.viewerBasisX.x, 5 ) + " " + fixedWidthNumberStringF( physarumConfig.viewerBasisX.y, 5 ) + " " + fixedWidthNumberStringF( physarumConfig.viewerBasisX.z, 5 ) ).flush() );
					terminal.addHistoryLine( terminal.csb.append( "Basis Y:  " + fixedWidthNumberStringF( physarumConfig.viewerBasisY.x, 5 ) + " " + fixedWidthNumberStringF( physarumConfig.viewerBasisY.y, 5 ) + " " + fixedWidthNumberStringF( physarumConfig.viewerBasisY.z, 5 ) ).flush() );
					terminal.addHistoryLine( terminal.csb.append( "Basis Z:  " + fixedWidthNumberStringF( physarumConfig.viewerBasisZ.x, 5 ) + " " + fixedWidthNumberStringF( physarumConfig.viewerBasisZ.y, 5 ) + " " + fixedWidthNumberStringF( physarumConfig.viewerBasisZ.z, 5 ) ).flush() );

				}, "Report the state of the viewer."
			);
		}
	}

	void HandleCustomEvents () {
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );
		// application specific controls

		// get new data into the input handler
		inputHandler.update();

		// pass any signals into the terminal
		terminal.update( inputHandler );

		// process input
		if ( !terminal.active ) {

			const bool shift = inputHandler.getState( KEY_RIGHT_SHIFT ) || inputHandler.getState( KEY_LEFT_SHIFT );
			const bool control = inputHandler.getState( KEY_RIGHT_CTRL ) || inputHandler.getState( KEY_LEFT_CTRL );

			if ( inputHandler.getState4( KEY_R ) == KEYSTATE_RISING ) {
				textureManager.ZeroTexture2D( "Accumulator" );
			}

			const float scalar = shift ? 10.0f : ( control ? 0.05f : 2.0f );
			if ( inputHandler.getState( KEY_W ) ) {
				glm::quat rot = glm::angleAxis( scalar / 100.0f, physarumConfig.viewerBasisX ); // basisX is the axis, therefore remains untransformed
				physarumConfig.viewerBasisY = ( rot * vec4( physarumConfig.viewerBasisY, 0.0f ) ).xyz();
				physarumConfig.viewerBasisZ = ( rot * vec4( physarumConfig.viewerBasisZ, 0.0f ) ).xyz();
			}
			if ( inputHandler.getState( KEY_S ) ) {
				glm::quat rot = glm::angleAxis( -scalar / 100.0f, physarumConfig.viewerBasisX );
				physarumConfig.viewerBasisY = ( rot * vec4( physarumConfig.viewerBasisY, 0.0f ) ).xyz();
				physarumConfig.viewerBasisZ = ( rot * vec4( physarumConfig.viewerBasisZ, 0.0f ) ).xyz();
			}
			if ( inputHandler.getState( KEY_A ) ) {
				glm::quat rot = glm::angleAxis( -scalar / 100.0f, physarumConfig.viewerBasisY ); // same as above, but basisY is the axis
				physarumConfig.viewerBasisX = ( rot * vec4( physarumConfig.viewerBasisX, 0.0f ) ).xyz();
				physarumConfig.viewerBasisZ = ( rot * vec4( physarumConfig.viewerBasisZ, 0.0f ) ).xyz();
			}
			if ( inputHandler.getState( KEY_D ) ) {
				glm::quat rot = glm::angleAxis( scalar / 100.0f, physarumConfig.viewerBasisY );
				physarumConfig.viewerBasisX = ( rot * vec4( physarumConfig.viewerBasisX, 0.0f ) ).xyz();
				physarumConfig.viewerBasisZ = ( rot * vec4( physarumConfig.viewerBasisZ, 0.0f ) ).xyz();
			}
			if ( inputHandler.getState( KEY_Q ) ) {
				glm::quat rot = glm::angleAxis( scalar / 100.0f, physarumConfig.viewerBasisZ ); // and again for basisZ
				physarumConfig.viewerBasisX = ( rot * vec4( physarumConfig.viewerBasisX, 0.0f ) ).xyz();
				physarumConfig.viewerBasisY = ( rot * vec4( physarumConfig.viewerBasisY, 0.0f ) ).xyz();
			}
			if ( inputHandler.getState( KEY_E ) ) {
				glm::quat rot = glm::angleAxis( -scalar / 100.0f, physarumConfig.viewerBasisZ );
				physarumConfig.viewerBasisX = ( rot * vec4( physarumConfig.viewerBasisX, 0.0f ) ).xyz();
				physarumConfig.viewerBasisY = ( rot * vec4( physarumConfig.viewerBasisY, 0.0f ) ).xyz();
			}
			// zoom in and out with plus/minus
			if ( inputHandler.getState( KEY_MINUS ) ) {
				physarumConfig.viewerFoV += scalar / 100.0f;
			}
			if ( inputHandler.getState( KEY_EQUALS ) ) {
				physarumConfig.viewerFoV -= scalar / 100.0f;
			}
			// f to reset basis, shift + f to reset basis and home to origin
			if ( inputHandler.getState( KEY_F ) ) {
				if ( shift ) physarumConfig.viewerPosition = vec3( 0.0f, 0.0f, 0.0f );
				physarumConfig.viewerBasisX = vec3( 1.0f, 0.0f, 0.0f );
				physarumConfig.viewerBasisY = vec3( 0.0f, 1.0f, 0.0f );
				physarumConfig.viewerBasisZ = vec3( 0.0f, 0.0f, 1.0f );
			}
			if ( inputHandler.getState( KEY_UP ) )			physarumConfig.viewerPosition += scalar * physarumConfig.viewerBasisZ;
			if ( inputHandler.getState( KEY_DOWN ) )		physarumConfig.viewerPosition -= scalar * physarumConfig.viewerBasisZ;
			if ( inputHandler.getState( KEY_RIGHT ) )		physarumConfig.viewerPosition += scalar * physarumConfig.viewerBasisX;
			if ( inputHandler.getState( KEY_LEFT ) )		physarumConfig.viewerPosition -= scalar * physarumConfig.viewerBasisX;
			if ( inputHandler.getState( KEY_PAGEDOWN ) )	physarumConfig.viewerPosition += scalar * physarumConfig.viewerBasisY;
			if ( inputHandler.getState( KEY_PAGEUP ) )		physarumConfig.viewerPosition -= scalar * physarumConfig.viewerBasisY;

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

		// expose controls for the physarum
		ImGui::Begin( "Physarum Controls" );

		ImGui::Checkbox( "Run Sim", &physarumConfig.runSim );

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

		static int currentPreset = 0;
		if ( ImGui::SmallButton( "Prev Preset" ) ) {
			currentPreset--;
			if ( currentPreset < 0 ) {
				currentPreset = presets.size() - 1;
			}
			ApplyPreset( currentPreset );
		}
		ImGui::SameLine();
		if ( ImGui::SmallButton( "Next Preset" ) ) {
			currentPreset = ( currentPreset + 1 ) % presets.size();
			ApplyPreset( currentPreset );
		}

		if ( ImGui::SmallButton( "Add Current Config To Presets" ) ) {
			// currentPreset will now be the index of the final element
			currentPreset = presets.size(); // don't need to decrement if you grab it before adding

			// add it to the live list
			preset_t current;
			current.senseAngle					= physarumConfig.senseAngle;
			current.senseDistance				= physarumConfig.senseDistance;
			current.turnAngle					= physarumConfig.turnAngle;
			current.stepSize					= physarumConfig.stepSize;
			current.decayFactor					= physarumConfig.decayFactor;
			current.depositAmount				= physarumConfig.depositAmount;
			current.writeBack					= physarumConfig.writeBack;
			presets.push_back( current );

			// add it to the config
			json j; ifstream i ( "src/projects/Physarum/presets.json" ); i >> j; i.close();
			json currentConfig;
			currentConfig[ "senseAngle" ] 		= current.senseAngle;
			currentConfig[ "senseDistance" ] 	= current.senseDistance;
			currentConfig[ "turnAngle" ] 		= current.turnAngle;
			currentConfig[ "stepSize" ]			= current.stepSize;
			currentConfig[ "decayFactor" ] 		= current.decayFactor;
			currentConfig[ "depositAmount" ]	= current.depositAmount;
			currentConfig[ "writeBack" ]		= current.writeBack;
			j[ "app" ][ "PhysarumPresets" ].push_back( currentConfig );
			std::ofstream o ( "src/projects/Physarum/presets.json" ); o << j.dump( 2 ); o.close();
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

		ImGui::Separator();
		ImGui::Text( "Rendering Density Threshold:" );
		ImGui::DragScalar( "   ", ImGuiDataType_S32, &physarumConfig.densityThreshold, 50, NULL, NULL, "%d units" );
		ImGui::Separator();
		ImGui::End();

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void DrawAPIGeometry () {
		ZoneScoped; scopedTimer Start( "API Geometry" );

	}

	void ComputePasses () {
		ZoneScoped;

		if ( physarumConfig.runSim ) { // update agents, held in the SSBO
			scopedTimer Start( "Agent Sim" );

			// send the simulation parameters
			const GLuint shader = shaders[ "Agents" ];
			glUseProgram( shader );
			glUniform1f( glGetUniformLocation( shader, "stepSize" ), physarumConfig.stepSize );
			glUniform1f( glGetUniformLocation( shader, "senseAngle" ), physarumConfig.senseAngle );
			glUniform1f( glGetUniformLocation( shader, "senseDistance" ), physarumConfig.senseDistance );
			glUniform1f( glGetUniformLocation( shader, "turnAngle" ), physarumConfig.turnAngle );
			glUniform1i( glGetUniformLocation( shader, "writeBack" ), physarumConfig.writeBack );
			glUniform1i( glGetUniformLocation( shader, "wangSeed" ), physarumConfig.wangSeed() );
			glUniform1ui( glGetUniformLocation( shader, "depositAmount" ), physarumConfig.depositAmount );
			glUniform1ui( glGetUniformLocation( shader, "numAgents" ), physarumConfig.numAgents );

			textureManager.BindImageForShader( string( "Pheremone Continuum Buffer " ) + string( physarumConfig.oddFrame ? "0" : "1" ), "current", shader, 2 );

			// number of 1024-size jobs, rounded up
			const int numSlices = ( physarumConfig.numAgents + 1023 ) / 1024;

			glDispatchCompute( 1, numSlices, 1 );
			glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );
		}

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();

			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			glUniform1i( glGetUniformLocation( shader, "wangSeed" ), physarumConfig.wangSeed() );
			glUniform3fv( glGetUniformLocation( shader, "viewerPosition" ), 1, glm::value_ptr( physarumConfig.viewerPosition ) );
			glUniform3fv( glGetUniformLocation( shader, "viewerBasisX" ), 1, glm::value_ptr( physarumConfig.viewerBasisX ) );
			glUniform3fv( glGetUniformLocation( shader, "viewerBasisY" ), 1, glm::value_ptr( physarumConfig.viewerBasisY ) );
			glUniform3fv( glGetUniformLocation( shader, "viewerBasisZ" ), 1, glm::value_ptr( physarumConfig.viewerBasisZ ) );
			glUniform1f( glGetUniformLocation( shader, "viewerFoV" ), physarumConfig.viewerFoV );
			glUniform1i( glGetUniformLocation( shader, "densityThreshold" ), physarumConfig.densityThreshold );

			textureManager.BindImageForShader( string( "Pheremone Continuum Buffer " ) + string( physarumConfig.oddFrame ? "1" : "0" ), "continuum", shader, 2 );

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
			textRenderer.Clear();
			textRenderer.Update( ImGui::GetIO().DeltaTime );

			// show terminal, if active - check happens inside
			textRenderer.drawTerminal( terminal );

			// put the result on the display
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

		if ( physarumConfig.runSim ) {
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

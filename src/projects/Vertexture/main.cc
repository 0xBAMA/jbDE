#include "../../engine/engine.h"
#include "vertextureModels.h"

class engineDemo : public engineBase {	// example derived class
public:
	engineDemo ( vertextureConfig set ) : gameConfig( set ) { Init(); OnInit(); PostInit(); }
	~engineDemo () { Quit(); }

	vertextureConfig gameConfig;

	GroundModel * ground;

	void OnInit () {
		ZoneScoped;
		{ Block Start( "Additional User Init" );
			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Background" ] = computeShader( "./src/projects/Vertexture/shaders/background.cs.glsl" ).shaderHandle;
			shaders[ "Ground" ] = regularShader( "./src/projects/Vertexture/shaders/ground.vs.glsl", "./src/projects/Vertexture/shaders/ground.fs.glsl" ).shaderHandle;

			// bindsets for the models

			// initialize game stuff
			ground = new GroundModel( shaders[ "Ground" ] );

			// default orientation
			trident.basisX = vec3(  0.610246f,  0.454481f,  0.648863f );
			trident.basisY = vec3(  0.791732f, -0.321969f, -0.519100f );
			trident.basisZ = vec3( -0.027008f,  0.830518f, -0.556314f );


		}
	}

	void HandleCustomEvents () {
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );
		// application specific controls
	}

	void ImguiPass () {
		ZoneScoped;

		// imgui control panel is going to be a big upgrade
			// also provides a place to show the eventReports

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

		ground->tridentRotation = glm::mat3(
			trident.basisX,
			trident.basisY,
			trident.basisZ
		);

		// draw some shit
		ground->Display();
	}

	void ComputePasses () {
		ZoneScoped;

		glActiveTexture( GL_TEXTURE0 + 0 ); // Texture unit 0
		glBindTexture( GL_TEXTURE_2D, textures[ "Display Texture" ] );

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Background" ] );
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
		ClearColorAndDepth();
		ComputePasses();			// draw the background
		BlitToScreen();				// fullscreen triangle copying to the screen
		DrawAPIGeometry();			// draw the API geometry - this is drawn over the background
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
		HandleQuitEvents(); // may have to skip this, tbd - I want to use keydown events to trigger behavior

		OnUpdate();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	vertextureConfig config;
	if ( argc == 5 ) {
		// cout << argv[0] << endl; // name of application
		config.numGoodGuys	= atoi( argv[ 1 ] );
		config.numBadGuys	= atoi( argv[ 2 ] );
		config.numTrees		= atoi( argv[ 3 ] );
		config.numBoxes		= atoi( argv[ 4 ] );
	} else {
		cout << "Incorrect # of command line args" << endl;
		cout << "  Game parameters defaulting" << endl;
	}

	engineDemo engineInstance ( config );
	while( !engineInstance.MainLoop() );
	return 0;
}

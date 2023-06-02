#include "../../engine/engine.h"
#include "vertexture.h"

class Vertexture2 : public engineBase {
public:
	Vertexture2 () { Init(); OnInit(); PostInit(); }
	~Vertexture2 () { Quit(); }

	// application data
	APIGeometryContainer data;

	// buffer locations are static, hardcoded so that we don't have to manage as much shit in the classes:

		// location 0 is the blue noise texture
		// location 1 is the steepness texture
		// location 2 is the distance/direction map
		// location 3 is the ssbo for the points
		// location 4 is the ssbo for the lights

		// and the rest with the samplers and shit is going to be passed as uniforms

	void OnInit () {
		ZoneScoped;
		{ Block Start( "Additional User Init" );

			// initial model orientation
			trident.basisX = vec3(  0.610246f,  0.454481f,  0.648863f );
			trident.basisY = vec3(  0.791732f, -0.321969f, -0.519100f );
			trident.basisZ = vec3( -0.027008f,  0.830518f, -0.556314f );

			// initialize the graphics api shit
			data.Reset();

		}
	}

	void HandleCustomEvents () {
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );
		// application specific controls

		const uint8_t *state = SDL_GetKeyboardState( NULL );
		if ( state[ SDL_SCANCODE_LEFTBRACKET ] )  { data.config.scale /= 0.99f; }
		if ( state[ SDL_SCANCODE_RIGHTBRACKET ] ) { data.config.scale *= 0.99f; }

		if ( state[ SDL_SCANCODE_R ] ) {
			data.Reset(); SDL_Delay( 40 ); // debounce
		}

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

		// ImGuiIO &io = ImGui::GetIO();
		// const float width = io.DisplaySize.x;
		// const float height = io.DisplaySize.y;

		// TODO: consider setting near and far clip planes based on current scale
			// currently, zoomed in, we run into a lot of clipping issues

		// draw the output
		data.Render();
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Background" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( data.resources.shaders[ "Background" ] );
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		// do the deferred update here
			// use the drawing bindset - I think this should go pretty smoothly once I have the Gbuffer

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

		// main view transform
		data.config.basisX = trident.basisX;
		data.config.basisY = trident.basisY;
		data.config.basisZ = trident.basisZ;

		// application-specific update code
		data.Update();
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
	// vertextureConfig config;
	// if ( argc == 5 ) {
	// 	// cout << argv[0] << endl; // name of application
	// 	config.numGoodGuys	= atoi( argv[ 1 ] );
	// 	config.numBadGuys	= atoi( argv[ 2 ] );
	// 	config.numTrees		= atoi( argv[ 3 ] );
	// 	config.numBoxes		= atoi( argv[ 4 ] );
	// } else {
	// 	cout << "Incorrect # of command line args" << endl;
	// 	cout << "  Game parameters defaulting" << endl;
	// }

	Vertexture2 engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

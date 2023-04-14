#include "../../engine/engine.h"
#include "vertextureModels.h"

class engineDemo : public engineBase {	// example derived class
public:
	engineDemo ( vertextureConfig set ) : gameConfig( set ) { Init(); OnInit(); PostInit(); }
	~engineDemo () { Quit(); }

	vertextureConfig gameConfig;

	// size scalar
	float scale = 1.0f;

	// application data
	GroundModel * ground;
	SkirtsModel * skirts;
	SphereModel * sphere;
	WaterModel * water;

	// shadowmapping resources
	GLuint shadowmapFramebuffer = 0;
	GLuint depthTexture;


	void OnInit () {
		ZoneScoped;
		{ Block Start( "Additional User Init" );

			// default orientation
			trident.basisX = vec3(  0.610246f,  0.454481f,  0.648863f );
			trident.basisY = vec3(  0.791732f, -0.321969f, -0.519100f );
			trident.basisZ = vec3( -0.027008f,  0.830518f, -0.556314f );

			// something to put some basic data in the accumulator
			shaders[ "Background" ] = computeShader( "./src/projects/Vertexture/shaders/background.cs.glsl" ).shaderHandle;
			shaders[ "Ground" ] = regularShader( "./src/projects/Vertexture/shaders/ground.vs.glsl", "./src/projects/Vertexture/shaders/ground.fs.glsl" ).shaderHandle;
			shaders[ "Sphere" ] = regularShader( "./src/projects/Vertexture/shaders/sphere.vs.glsl", "./src/projects/Vertexture/shaders/sphere.fs.glsl" ).shaderHandle;
			shaders[ "Sphere Movement" ] = computeShader( "./src/projects/Vertexture/shaders/movingSphere.cs.glsl" ).shaderHandle;
			shaders[ "Sphere Map Update" ] = computeShader( "./src/projects/Vertexture/shaders/movingSphereMaps.cs.glsl" ).shaderHandle;
			shaders[ "Moving Sphere" ] = regularShader( "./src/projects/Vertexture/shaders/movingSphere.vs.glsl", "./src/projects/Vertexture/shaders/movingSphere.fs.glsl" ).shaderHandle;
			shaders[ "Water" ] = regularShader( "./src/projects/Vertexture/shaders/water.vs.glsl", "./src/projects/Vertexture/shaders/water.fs.glsl" ).shaderHandle;
			shaders[ "Skirts" ] = regularShader( "./src/projects/Vertexture/shaders/skirts.vs.glsl", "./src/projects/Vertexture/shaders/skirts.fs.glsl" ).shaderHandle;

			GLuint steepness, distanceDirection;
			Image_4U steepnessTex( 512, 512 );

			glGenTextures( 1, &steepness );
			glActiveTexture( GL_TEXTURE14 );
			glBindTexture( GL_TEXTURE_2D, steepness );
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, steepnessTex.GetImageDataBasePtr() );
			textures[ "Steepness Map" ] = steepness;

			glGenTextures( 1, &distanceDirection );
			glActiveTexture( GL_TEXTURE15 );
			glBindTexture( GL_TEXTURE_2D, distanceDirection );
			glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, steepnessTex.GetImageDataBasePtr() );
			textures[ "Distance/Direction Map" ] = distanceDirection;

			// =================================================================================================

			// create the shadowmap resources
			GLuint shadowmapFramebuffer = 0;
			glGenFramebuffers( 1, &shadowmapFramebuffer );
			glBindFramebuffer( GL_FRAMEBUFFER, shadowmapFramebuffer );

			// Depth texture - slower than a depth buffer, but you can sample it later in your shader
			glGenTextures( 1, &depthTexture );
			glBindTexture( GL_TEXTURE_2D, depthTexture );
			glTexImage2D( GL_TEXTURE_2D, 0,GL_DEPTH_COMPONENT16, 1024, 1024, 0,GL_DEPTH_COMPONENT, GL_FLOAT, 0 );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

			glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture, 0 );
			glDrawBuffer( GL_NONE ); // No color buffer is drawn to.

			if( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
				cout << "framebuffer creation failed" << endl; abort();
			}

			// revert to default framebuffer
			glBindFramebuffer( GL_FRAMEBUFFER, 0 );

			// =================================================================================================

			palette::PickRandomPalette();

			// initialize game stuff
			ground = new GroundModel( shaders[ "Ground" ] );
			textures[ "Ground" ] = ground->heightmap;

			skirts = new SkirtsModel( shaders[ "Skirts" ] );
			skirts->groundColor = ground->groundColor;

			sphere = new SphereModel( shaders[ "Sphere" ], shaders[ "Moving Sphere" ], shaders[ "Sphere Movement" ], gameConfig.numTrees );
			sphere->steepness = textures[ "Steepness Map" ];
			sphere->distanceDirection = textures[ "Distance/Direction Map" ];
			sphere->mapUpdateShader = shaders[ "Sphere Map Update" ];

			water = new WaterModel( shaders[ "Water" ] );

		}
	}

	void HandleCustomEvents () {
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );
		// application specific controls

		const uint8_t *state = SDL_GetKeyboardState( NULL );
		if ( state[ SDL_SCANCODE_LEFTBRACKET ] )  { scale *= 1.01f; }
		if ( state[ SDL_SCANCODE_RIGHTBRACKET ] ) { scale *= 0.99f; }

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

		ImGuiIO &io = ImGui::GetIO();
		const float width = io.DisplaySize.x;
		const float height = io.DisplaySize.y;
		// screen aspect ratio
		skirts->screenAR = water->screenAR = sphere->screenAR = ground->screenAR = width / height;

		// scale adjustment
		skirts->scale = water->scale = sphere->scale = ground->scale = scale;

		// display model transform
		skirts->tridentM = water->tridentM = sphere->tridentM = ground->tridentM = glm::mat3(
			trident.basisX,
			trident.basisY,
			trident.basisZ
		);

		// shadowmap model transform
		// skirts->tridentD = water->tridentD = sphere->tridentD = ground->tridentD = glm::mat3(
		// 	tridentDepth.basisX,
		// 	tridentDepth.basisY,
		// 	tridentDepth.basisZ
		// );

		// prepare to render the shadowmap depth
		glBindFramebuffer( GL_FRAMEBUFFER, shadowmapFramebuffer );

		// get shadow depth
		ground->ShadowDisplay();
		sphere->ShadowDisplay();
		water->ShadowDisplay();
		skirts->ShadowDisplay();

		// revert to default framebuffer
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );

		// draw the output
		ground->Display();
		sphere->Display();
		water->Display();
		skirts->Display();
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Background" );
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
		static int counter = 0;
		counter++;
		// ground->Update( counter );
		// skirts->Update( counter );
		sphere->Update( counter );
		// water->Update( counter );
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

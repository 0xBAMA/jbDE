#include "engine.h"
#include "../utils/debug/debug.h"

void engineBase::StartMessage () {
	ZoneScoped;
	cout << endl << T_YELLOW << BOLD << "  jbDE - the jb Demo Engine" << newline;
	cout << " By Jon Baker ( 2020 - 2023 ) " << RESET << newline;
	cout << "  https://jbaker.graphics/ " << newline << newline;
}

void engineBase::LoadConfig () {
	ZoneScoped;

	{	Block Start( "Configuring Application" );

		json j;
		// load the config json, populate config struct - this will probably have more data, eventually
		ifstream i( "src/engine/config.json" );
		i >> j; i.close();
		config.windowTitle				= j[ "windowTitle" ];
		config.width					= j[ "screenWidth" ];
		config.height					= j[ "screenHeight" ];
		config.linearFilter				= j[ "linearFilterDisplayTex" ];
		config.windowOffset.x			= j[ "windowOffset" ][ "x" ];
		config.windowOffset.y			= j[ "windowOffset" ][ "y" ];
		config.startOnScreen			= j[ "startOnScreen" ];
		config.numMsDelayAfterCallback	= j[ "numMsDelayAfterCallback" ];
		config.windowFlags				|= ( j[ "SDL_WINDOW_FULLSCREEN" ] ? SDL_WINDOW_FULLSCREEN : 0 );
		config.windowFlags				|= ( j[ "SDL_WINDOW_FULLSCREEN_DESKTOP" ] ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0 );
		config.windowFlags				|= ( j[ "SDL_WINDOW_BORDERLESS" ] ? SDL_WINDOW_BORDERLESS : 0 );
		config.windowFlags				|= ( j[ "SDL_WINDOW_RESIZABLE" ] ? SDL_WINDOW_RESIZABLE : 0 );
		config.windowFlags				|= ( j[ "SDL_WINDOW_INPUT_GRABBED" ] ? SDL_WINDOW_INPUT_GRABBED : 0 );
		config.vSyncEnable				= j[ "vSyncEnable" ];
		config.MSAACount				= j[ "MSAACount" ];
		config.OpenGLVersionMajor		= j[ "OpenGLVersionMajor" ];
		config.OpenGLVersionMinor		= j[ "OpenGLVersionMinor" ];
		config.OpenGLVerboseInit		= j[ "OpenGLVerboseInit" ];
		config.SDLDisplayModeDump		= j[ "SDLDisplayModeDump" ];
		config.reportPlatformInfo		= j[ "reportPlatformInfo" ];
		config.enableDepthTesting		= j[ "depthTesting" ];
		config.pointSize				= j[ "pointSize" ];
		config.SRGBFramebuffer			= j[ "SRGBFramebuffer" ];
		config.clearColor.r				= j[ "clearColor" ][ "r" ];
		config.clearColor.g				= j[ "clearColor" ][ "g" ];
		config.clearColor.b				= j[ "clearColor" ][ "b" ];
		config.clearColor.a				= j[ "clearColor" ][ "a" ];

		config.oneShot					= j[ "oneShot" ]; // relatively special purpose - run intialization, and one pass through the main loop before quitting
		showDemoWindow					= j[ "showImGUIDemoWindow" ];
		showProfiler					= j[ "showProfiler" ];

		// color grading stuff
		tonemap.tonemapMode				= j[ "colorGrade" ][ "tonemapMode" ];
		tonemap.gamma					= j[ "colorGrade" ][ "gamma" ];
		tonemap.colorTemp				= j[ "colorGrade" ][ "colorTemp" ];
	}
}

void engineBase::CreateWindowAndContext () {
	ZoneScoped;

	window.config = &config;

	{ Block Start( "Initializing SDL2" ); window.PreInit(); }
	{ Block Start( "Creating Window" ); window.Init(); }
	{ Block Start( "Setting Up OpenGL Context" ); window.OpenGLSetup(); }

	// setup OpenGL debug callback with configured delay
	numMsDelayAfterCallback = config.numMsDelayAfterCallback;
	GLDebugEnable();
	cout << endl << T_YELLOW << "  OpenGL debug callback enabled - " << numMsDelayAfterCallback << "ms delay.\n" << RESET << endl;
}

// split up into vertex, texture funcs + report platform info ( maybe do this later? )
void engineBase::DisplaySetup () {
	ZoneScoped;

	// some info on your current platform
	if ( config.reportPlatformInfo ) {
		const GLubyte *vendor = glGetString( GL_VENDOR );
		const GLubyte *renderer = glGetString( GL_RENDERER );
		const GLubyte *version = glGetString( GL_VERSION );
		const GLubyte *glslVersion = glGetString( GL_SHADING_LANGUAGE_VERSION );
		cout << T_BLUE << "    Platform Info :" << RESET << newline;
		cout << T_RED << "      Vendor : " << T_CYAN << vendor << RESET << newline;
		cout << T_RED << "      Renderer : " << T_CYAN << renderer << RESET << newline;
		cout << T_RED << "      OpenGL Version Supported : " << T_CYAN << version << RESET << newline;
		cout << T_RED << "      GLSL Version Supported : " << T_CYAN << glslVersion << RESET << newline << newline;
	}

	if ( config.OpenGLVerboseInit ) {
		// report all gl extensions - useful on different platforms
		GLint n;
		glGetIntegerv( GL_NUM_EXTENSIONS, &n );
		cout << "starting dump of " << n << " extensions" << endl;
		for ( int i = 0; i < n; i++ )
		cout << "\t" << i << " : " << glGetStringi( GL_EXTENSIONS, i ) << endl;
		cout << endl;
	}

	if ( config.SDLDisplayModeDump ) {
		static int display_in_use = 0; /* Only using first display */

		int i, display_mode_count;
		SDL_DisplayMode mode;
		Uint32 f;

		SDL_Log("SDL_GetNumVideoDisplays(): %i", SDL_GetNumVideoDisplays());

		display_mode_count = SDL_GetNumDisplayModes(display_in_use);
		if (display_mode_count < 1) {
			SDL_Log("SDL_GetNumDisplayModes failed: %s", SDL_GetError());
			// return 1;
		}
		SDL_Log("SDL_GetNumDisplayModes: %i", display_mode_count);

		for (i = 0; i < display_mode_count; ++i) {
			if (SDL_GetDisplayMode(display_in_use, i, &mode) != 0) {
			SDL_Log("SDL_GetDisplayMode failed: %s", SDL_GetError());
			// return 1;
		}
		f = mode.format;

		SDL_Log("Mode %i\tbpp %i\t%s\t%i x %i",
			i, SDL_BITSPERPIXEL(f),
			SDL_GetPixelFormatName(f),
			mode.w, mode.h);
		}
	}
}

void engineBase::SetupVertexData () {
	ZoneScoped;

	{	Block Start( "Setting Up Vertex Data" );

		// OpenGL core spec requires a VAO bound when calling glDrawArrays
		glGenVertexArrays( 1, &displayVAO );
		glBindVertexArray( displayVAO );

		// corresponding VBO, unused by the default jbDE template
		glGenBuffers( 1, &displayVBO );
		glBindBuffer( GL_ARRAY_BUFFER, displayVBO );

		// no op, by default...
			// load models, setup vertex attributes, etc, here
	}
}

void engineBase::SetupTextureData () {
	ZoneScoped;

	{	Block Start( "Setting Up Textures" );

		textureManager.Init();

		GLuint accumulatorTexture;
		GLuint displayTexture;
		GLuint blueNoiseTexture;
		GLuint tridentImage;
		GLuint bayer2, bayer4, bayer8;

		// create the image textures
		Image_4U initial( config.width, config.height );
		glGenTextures( 1, &accumulatorTexture );
		glActiveTexture( GL_TEXTURE3 );
		glBindTexture( GL_TEXTURE_2D, accumulatorTexture );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, config.width, config.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, initial.GetImageDataBasePtr() );
		textures[ "Accumulator" ] = accumulatorTexture;

		glGenTextures( 1, &displayTexture );
		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_2D, displayTexture );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, config.linearFilter ? GL_LINEAR : GL_NEAREST );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, config.linearFilter ? GL_LINEAR : GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, config.width, config.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, initial.GetImageDataBasePtr() );
		textures[ "Display Texture" ] = displayTexture;

		// blue noise image on the GPU
		Image_4U blueNoiseImage{ "src/utils/noise/blueNoise.png" };
		glGenTextures( 1, &blueNoiseTexture );
		glActiveTexture( GL_TEXTURE4 );
		glBindTexture( GL_TEXTURE_2D, blueNoiseTexture );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, blueNoiseImage.Width(), blueNoiseImage.Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, blueNoiseImage.GetImageDataBasePtr() );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		textures[ "Blue Noise" ] = blueNoiseTexture;

		// create the image for the trident
		Image_4U initialT( trident.blockDimensions.x * 8, trident.blockDimensions.y * 16 );
		glGenTextures( 1, &tridentImage );
		glActiveTexture( GL_TEXTURE5 );
		glBindTexture( GL_TEXTURE_2D, tridentImage );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, initialT.Width(), initialT.Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, initialT.GetImageDataBasePtr() );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		trident.PassInImage( tridentImage );
		textures[ "Trident" ] = tridentImage;

		// bayer patterns
		glActiveTexture( GL_TEXTURE6 );
		glGenTextures( 1, &bayer2 );
		glBindTexture( GL_TEXTURE_2D, bayer2 );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, &Make4Channel( BayerData( 2 ) )[ 0 ] );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		textures[ "Bayer2" ] = bayer2;

		glActiveTexture( GL_TEXTURE7 );
		glGenTextures( 1, &bayer4 );
		glBindTexture( GL_TEXTURE_2D, bayer4 );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, &Make4Channel( BayerData( 4 ) )[ 0 ] );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		textures[ "Bayer4" ] = bayer4;

		glActiveTexture( GL_TEXTURE8 );
		glGenTextures( 1, &bayer8 );
		glBindTexture( GL_TEXTURE_2D, bayer8 );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, &Make4Channel( BayerData( 8 ) )[ 0 ] );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		textures[ "Bayer8" ] = bayer8;

	}

	{	Block Start( "Setting Up Bindsets" );

		bindSets[ "Drawing" ] = bindSet( {
			binding( 0, textures[ "Blue Noise" ], GL_RGBA8UI ),
			binding( 1, textures[ "Accumulator" ], GL_RGBA16F )
		} );

		bindSets[ "Postprocessing" ] = bindSet( {
			binding( 0, textures[ "Accumulator" ], GL_RGBA16F ),
			binding( 1, textures[ "Display Texture" ], GL_RGBA8UI )
		} );

		bindSets[ "Display" ] = bindSet( {
			binding( 0, textures[ "Display Texture" ], GL_RGBA8UI )
		} );
	}
}

void engineBase::LoadData () {
	ZoneScoped;

	{ Block Start( "Loading Palettes" );
		LoadPalettes( paletteList );
		palette::PopulateLocalList( paletteList );
	}

	{ Block Start( "Loading Font Glyphs" );
		LoadGlyphs( glyphList );
	}

	{ Block Start( "Load Wordlists" );
		LoadBadWords( badWords );
		LoadColorWords( colorWords );

		/* plantWords? tbd */
	}
}

void engineBase::ShaderCompile () {
	ZoneScoped;

	{	Block Start( "Compiling Shaders" );

		const string base( "./src/engine/shaders/" );

		// tonemapping shader - this will need some refinement as time goes on
		shaders[ "Tonemap" ] = computeShader( base + "tonemap.cs.glsl" ).shaderHandle;

		// create the shader for the triangles to cover the screen
		shaders[ "Display" ] = regularShader( base + "blit.vs.glsl", base + "blit.fs.glsl" ).shaderHandle;

		// initialize the text renderer
		shaders[ "Font Renderer" ] = computeShader( "src/utils/fonts/fontRenderer/font.cs.glsl" ).shaderHandle;
		textRenderer.Init( config.width, config.height, shaders[ "Font Renderer" ] );

		// trident shaders
		shaders[ "Trident Raymarch" ] = computeShader( "./src/utils/trident/tridentGenerate.cs.glsl" ).shaderHandle;
		shaders[ "Trident Blit" ] = computeShader( "./src/utils/trident/tridentCopy.cs.glsl" ).shaderHandle;
		trident.basePt = textRenderer.basePt; // location to draw at
		trident.PassInShaders( shaders[ "Trident Raymarch" ], shaders[ "Trident Blit" ] );
	}
}

void engineBase::ImguiSetup () {
	ZoneScoped;

	{	Block Start( "Configuring dearImGUI" );

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO();

		// enable docking
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		// Setup Platform/Renderer bindings
		ImGui_ImplSDL2_InitForOpenGL( window.window, window.GLcontext );
		const char *glsl_version = "#version 430";
		ImGui_ImplOpenGL3_Init( glsl_version );

		// setting custom font, if desired
		// io.Fonts->AddFontFromFileTTF( "src/fonts/star_trek/titles/TNG_Title.ttf", 16 );

		ImVec4 *colors = ImGui::GetStyle().Colors;
		colors[ ImGuiCol_Text ]						= ImVec4( 0.64f, 0.37f, 0.37f, 1.00f );
		colors[ ImGuiCol_TextDisabled ]				= ImVec4( 0.49f, 0.26f, 0.26f, 1.00f );
		colors[ ImGuiCol_WindowBg ]					= ImVec4( 0.17f, 0.00f, 0.00f, 0.98f );
		colors[ ImGuiCol_ChildBg ]					= ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
		colors[ ImGuiCol_PopupBg ]					= ImVec4( 0.18f, 0.00f, 0.00f, 0.94f );
		colors[ ImGuiCol_Border ]					= ImVec4( 0.35f, 0.00f, 0.03f, 0.50f );
		colors[ ImGuiCol_BorderShadow ]				= ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
		colors[ ImGuiCol_FrameBg ]					= ImVec4( 0.14f, 0.04f, 0.00f, 1.00f );
		colors[ ImGuiCol_FrameBgHovered ]			= ImVec4( 0.14f, 0.04f, 0.00f, 1.00f );
		colors[ ImGuiCol_FrameBgActive ]			= ImVec4( 0.14f, 0.04f, 0.00f, 1.00f );
		colors[ ImGuiCol_TitleBg ]					= ImVec4( 0.14f, 0.04f, 0.00f, 1.00f );
		colors[ ImGuiCol_TitleBgActive ]			= ImVec4( 0.14f, 0.04f, 0.00f, 1.00f );
		colors[ ImGuiCol_TitleBgCollapsed ]			= ImVec4( 0.00f, 0.00f, 0.00f, 0.51f );
		colors[ ImGuiCol_MenuBarBg ]				= ImVec4( 0.14f, 0.14f, 0.14f, 1.00f );
		colors[ ImGuiCol_ScrollbarBg ]				= ImVec4( 0.02f, 0.02f, 0.02f, 0.53f );
		colors[ ImGuiCol_ScrollbarGrab ]			= ImVec4( 0.31f, 0.31f, 0.31f, 1.00f );
		colors[ ImGuiCol_ScrollbarGrabHovered ]		= ImVec4( 0.41f, 0.41f, 0.41f, 1.00f );
		colors[ ImGuiCol_ScrollbarGrabActive ]		= ImVec4( 0.51f, 0.51f, 0.51f, 1.00f );
		colors[ ImGuiCol_CheckMark ]				= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
		colors[ ImGuiCol_SliderGrab ]				= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
		colors[ ImGuiCol_SliderGrabActive ]			= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
		colors[ ImGuiCol_Button ]					= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
		colors[ ImGuiCol_ButtonHovered ]			= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
		colors[ ImGuiCol_ButtonActive ]				= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
		colors[ ImGuiCol_Header ]					= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
		colors[ ImGuiCol_HeaderHovered ]			= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
		colors[ ImGuiCol_HeaderActive ]				= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
		colors[ ImGuiCol_Separator ]				= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
		colors[ ImGuiCol_SeparatorHovered ]			= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
		colors[ ImGuiCol_SeparatorActive ]			= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
		colors[ ImGuiCol_ResizeGrip ]				= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
		colors[ ImGuiCol_ResizeGripHovered ]		= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
		colors[ ImGuiCol_ResizeGripActive ]			= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
		colors[ ImGuiCol_Tab ]						= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
		colors[ ImGuiCol_TabHovered ]				= ImVec4( 0.87f, 0.23f, 0.09f, 1.00f );
		colors[ ImGuiCol_TabActive ]				= ImVec4( 1.00f, 0.33f, 0.00f, 1.00f );
		colors[ ImGuiCol_TabUnfocused ]				= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
		colors[ ImGuiCol_TabUnfocusedActive ]		= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
		colors[ ImGuiCol_PlotLines ]				= ImVec4( 0.61f, 0.61f, 0.61f, 1.00f );
		colors[ ImGuiCol_PlotLinesHovered ]			= ImVec4( 1.00f, 0.43f, 0.35f, 1.00f );
		colors[ ImGuiCol_PlotHistogram ]			= ImVec4( 0.90f, 0.70f, 0.00f, 1.00f );
		colors[ ImGuiCol_PlotHistogramHovered ]		= ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
		colors[ ImGuiCol_TextSelectedBg ]			= ImVec4( 0.81f, 0.38f, 0.09f, 0.08f );
		colors[ ImGuiCol_DragDropTarget ]			= ImVec4( 1.00f, 1.00f, 0.00f, 0.90f );
		colors[ ImGuiCol_NavHighlight ]				= ImVec4( 1.00f, 0.60f, 0.00f, 1.00f );
		colors[ ImGuiCol_NavWindowingHighlight ]	= ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
		colors[ ImGuiCol_NavWindowingDimBg ]		= ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
		colors[ ImGuiCol_ModalWindowDimBg ]			= ImVec4( 0.80f, 0.80f, 0.80f, 0.35f );
	}
}

void engineBase::InitialClear () {
	ZoneScoped;

	{	Block Start( "Clear Buffer" );

		glClearColor( config.clearColor.x, config.clearColor.y, config.clearColor.z, config.clearColor.w );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		window.Swap();
	}
}

void engineBase::ReportStartupStats () {
	ZoneScoped;

	cout << endl << T_CYAN << "  " <<
		shaders.size() << " shader programs, " <<
		textures.size() << " textures, " <<
		bindSets.size() << " bindsets" << endl;

	cout << T_YELLOW << "  Startup is complete ( total " << TotalTime() << "ms )" << RESET << endl << endl;
}

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

	{
		Block Start( "Configuring Application" );

		// load the config json, populate config struct - this will probably have more data, eventually
		json j; ifstream i( "src/engine/config.json" ); i >> j; i.close();
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
		config.SRGBFramebuffer			= j[ "SRGBFramebuffer" ];
		config.loadDataResources		= j[ "loadDataResources" ];
		config.clearColor.r				= j[ "clearColor" ][ "r" ];
		config.clearColor.g				= j[ "clearColor" ][ "g" ];
		config.clearColor.b				= j[ "clearColor" ][ "b" ];
		config.clearColor.a				= j[ "clearColor" ][ "a" ];

		config.oneShot					= j[ "oneShot" ]; // relatively special purpose - run intialization, and one pass through the main loop before quitting
		showDemoWindow					= j[ "showImGUIDemoWindow" ];
		showProfiler					= j[ "showProfiler" ];

		// color grading stuff
		tonemap.showTonemapWindow		= j[ "colorGrade" ][ "showTonemapWindow" ];
		tonemap.tonemapMode				= j[ "colorGrade" ][ "tonemapMode" ];
		tonemap.gamma					= j[ "colorGrade" ][ "gamma" ];
		tonemap.colorTemp				= j[ "colorGrade" ][ "colorTemp" ];
	}
}

void engineBase::CreateWindowAndContext () {
	ZoneScoped;

	window.config = &config;

	{
		Block Start( "Initializing SDL2" );
		window.PreInit();
	}

	{
		Block Start( "Creating Window" );
		window.Init();
	}

	{
		Block Start( "Setting Up OpenGL Context" );
		window.OpenGLSetup();
	}

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
		// report current platform
		cout << T_BLUE << "    Platform : " << T_CYAN << SDL_GetPlatform() << RESET << newline << newline;

		// CPU information
		cout << T_BLUE << "    CPU Info :" << RESET << newline;
		cout << T_RED << "      Logical Cores : " << T_CYAN << SDL_GetCPUCount() << newline;
		cout << T_RED << "      System RAM : " << T_CYAN << SDL_GetSystemRAM() << " MB" << newline;
		cout << newline;

		// GPU information from OpenGL
		const GLubyte *vendor = glGetString( GL_VENDOR );
		const GLubyte *renderer = glGetString( GL_RENDERER );
		const GLubyte *version = glGetString( GL_VERSION );
		const GLubyte *glslVersion = glGetString( GL_SHADING_LANGUAGE_VERSION );
		cout << T_BLUE << "    GPU Info :" << RESET << newline;
		cout << T_RED << "      Vendor : " << T_CYAN << vendor << RESET << newline;
		cout << T_RED << "      Renderer : " << T_CYAN << renderer << RESET << newline;
		cout << T_RED << "      OpenGL Version Supported : " << T_CYAN << version << RESET << newline;
		cout << T_RED << "      GLSL Version Supported : " << T_CYAN << glslVersion << RESET << newline << newline;

		// https://wiki.libsdl.org/SDL2/SDL_GetNumVideoDrivers
		// https://wiki.libsdl.org/SDL2/SDL_GetCurrentVideoDriver
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
		// https://wiki.libsdl.org/SDL2/SDL_GetDesktopDisplayMode
		static int display_in_use = 0;
		int i, display_mode_count;
		SDL_DisplayMode mode;
		Uint32 f;

		SDL_Log( "SDL_GetNumVideoDisplays(): %i", SDL_GetNumVideoDisplays() );

		display_mode_count = SDL_GetNumDisplayModes( display_in_use );
		if ( display_mode_count < 1 ) {
			SDL_Log( "SDL_GetNumDisplayModes failed: %s", SDL_GetError() );
		}
		SDL_Log( "SDL_GetNumDisplayModes: %i", display_mode_count );

		for ( i = 0; i < display_mode_count; ++i ) {
			if ( SDL_GetDisplayMode( display_in_use, i, &mode ) != 0 ) {
				SDL_Log( "SDL_GetDisplayMode failed: %s", SDL_GetError() );
			}
			f = mode.format;
			SDL_Log( "Mode %i\tbpp %i\t%s\t%i x %i",
				i, SDL_BITSPERPIXEL( f ),
				SDL_GetPixelFormatName( f ),
				mode.w, mode.h );
		}
	}
}

void engineBase::SetupVertexData () {
	ZoneScoped;

	{
		Block Start( "Setting Up Vertex Data" );

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

	{
		Block Start( "Setting Up Textures" );

		textureManager.Init();
		textureOptions_t opts;

	// =======================================================================
		// LDR tonemapped texture, ready for display
		opts.width = config.width;
		opts.height = config.height;
		opts.dataType = GL_RGBA8;
		opts.minFilter = ( config.linearFilter ? GL_LINEAR : GL_NEAREST );
		opts.magFilter = ( config.linearFilter ? GL_LINEAR : GL_NEAREST );
		opts.textureType = GL_TEXTURE_2D;
		opts.pixelDataType = GL_UNSIGNED_BYTE;
		opts.initialData = nullptr;

		textureManager.Add( "Display Texture", opts );
	// =======================================================================

	// =======================================================================
		// holds renderer state at high bit depth
		opts.width = config.width;
		opts.height = config.height;
		opts.dataType = GL_RGBA16F;
		opts.minFilter = GL_NEAREST;
		opts.magFilter = GL_NEAREST;
		opts.textureType = GL_TEXTURE_2D;
		opts.pixelDataType = GL_UNSIGNED_BYTE;
		opts.initialData = nullptr;

		textureManager.Add( "Accumulator", opts );
	// =======================================================================

	// =======================================================================
		// blue noise texture, used for various purposes
		Image_4U blueNoiseImage{ "src/utils/noise/blueNoise.png" };
		opts.width = blueNoiseImage.Width();
		opts.height = blueNoiseImage.Height();
		opts.dataType = GL_RGBA8UI;
		opts.minFilter = GL_NEAREST;
		opts.magFilter = GL_NEAREST;
		opts.textureType = GL_TEXTURE_2D;
		opts.pixelDataType = GL_UNSIGNED_BYTE;
		opts.initialData = ( void * ) blueNoiseImage.GetImageDataBasePtr();

		textureManager.Add( "Blue Noise", opts );
	// =======================================================================

	// =======================================================================
		// add textures for the text renderer + access to the manager
		textRenderer.textureManager_local = &textureManager;
		textRenderer.Init( config.width, config.height, shaders[ "Font Renderer" ] );

		// font atlas texture
		Image_4U fontAtlas( "./src/utils/fonts/fontRenderer/whiteOnClear.png" );
		fontAtlas.FlipVertical();
		opts.width = fontAtlas.Width();
		opts.height = fontAtlas.Height();
		opts.dataType = GL_RGBA8UI;
		opts.pixelDataType = GL_UNSIGNED_BYTE;
		opts.initialData = ( void * ) fontAtlas.GetImageDataBasePtr();

		// font renderer data textures created internal to each layer via textureManager_local
		textureManager.Add( "Font Atlas", opts );
	// =======================================================================

	// =======================================================================
		// image buffer for the trident orientation indicator
		opts.width = trident.blockDimensions.x * 8;
		opts.height = trident.blockDimensions.y * 16;
		opts.minFilter = GL_NEAREST;
		opts.magFilter = GL_NEAREST;

		textureManager.Add( "Trident Storage", opts );
		trident.PassInImage( textureManager.Get( "Trident Storage" ) );
		trident.basePt = textRenderer.basePt; // location to draw at
	// =======================================================================

	// =======================================================================
		// bayer patterns todo - how important is it that they're loaded at startup?
	// =======================================================================

	}

	{
		Block Start( "Setting Up Bindsets" );

		// this will need some work

		bindSets[ "Drawing" ] = bindSet( {
			binding( 0, textureManager.Get( "Blue Noise" ), GL_RGBA8UI ),
			binding( 1, textureManager.Get( "Accumulator" ), GL_RGBA16F )
		} );

		bindSets[ "Postprocessing" ] = bindSet( {
			binding( 0, textureManager.Get( "Accumulator" ), GL_RGBA16F ),
			binding( 1, textureManager.Get( "Display Texture" ), GL_RGBA8UI )
		} );

		bindSets[ "Display" ] = bindSet( {
			binding( 0, textureManager.Get( "Display Texture" ), GL_RGBA8UI )
		} );
	}
}

void engineBase::LoadData () {
	ZoneScoped;

	if ( config.loadDataResources ) { // toggle loading of palettes, font glyphs, and bad/color wordlists
		{
			Block Start( "Loading Palettes" );

			LoadPalettes( paletteList );
			palette::PopulateLocalList( paletteList );
			// cout << "loaded " << paletteList.size() << " palettes" << newline;
		}

		{
			Block Start( "Loading Font Glyphs" );

			LoadGlyphs( glyphList );
			// cout << "loaded " << glyphList.size() << " glyphs" << newline;
		}

		{
			Block Start( "Load Wordlists" );

			LoadBadWords( badWords );
			// cout << "loaded " << badWords.size() << " bad words" << newline;

			LoadColorWords( colorWords );
			// cout << "loaded " << colorWords.size() << " color words" << newline;

			/* plantWords, animalWords, etc? tbd */
		}
	}
}

void engineBase::ShaderCompile () {
	ZoneScoped;

	{
		Block Start( "Compiling Shaders" );

		const string basePath( "./src/engine/shaders/" );

		// tonemapping shader - this will need some refinement as time goes on
		shaders[ "Tonemap" ] = computeShader( basePath + "tonemap.cs.glsl" ).shaderHandle;

		// create the shader for the triangles to cover the screen
		shaders[ "Display" ] = regularShader( basePath + "blit.vs.glsl", basePath + "blit.fs.glsl" ).shaderHandle;

		// initialize the text renderer
		shaders[ "Font Renderer" ] = computeShader( "src/utils/fonts/fontRenderer/font.cs.glsl" ).shaderHandle;

		// trident shaders
		shaders[ "Trident Raymarch" ] = computeShader( "./src/utils/trident/tridentGenerate.cs.glsl" ).shaderHandle;
		shaders[ "Trident Blit" ] = computeShader( "./src/utils/trident/tridentCopy.cs.glsl" ).shaderHandle;
		trident.PassInShaders( shaders[ "Trident Raymarch" ], shaders[ "Trident Blit" ] );
	}
}

void engineBase::ImguiSetup () {
	ZoneScoped;

	{
		Block Start( "Configuring dearImGUI" );

		// Setup Dear ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO();

		// enable docking
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

		// Setup Platform/Renderer bindings
		ImGui_ImplSDL2_InitForOpenGL( window.window, window.GLcontext );
		const char *glsl_version = "#version 450";
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

	{
		Block Start( "Clear Buffer" );
		glClearColor( config.clearColor.x, config.clearColor.y, config.clearColor.z, config.clearColor.w );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		window.Swap();
	}
}

void engineBase::ReportStartupStats () {
	ZoneScoped;

	cout << endl << T_CYAN << "  " <<
		shaders.size() << " shader programs, " <<
		bindSets.size() << " bindsets" << endl;

	const size_t bytes = textureManager.TotalSize();
	cout << "  " << textureManager.Count() << " textures " << float( bytes ) / float( 1u << 20 ) << "MB ( " << GetWithThousandsSeparator( bytes ) << " bytes )" << endl;
	cout << T_YELLOW << "  Startup is complete ( total " << TotalTime() << " ms )" << RESET << endl << endl;

	// texture setup report
	textureManager.EnumerateTextures();
}

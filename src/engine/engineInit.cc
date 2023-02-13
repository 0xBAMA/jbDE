#include "engine.h"
#include "../debug/debug.h"

class Block {
const int reportWidth = 64;
public:
	Block( string sectionName ) {
		Tick();
		cout << T_BLUE << "    " << sectionName << " " << RESET;
		for ( unsigned int i = 0; i < reportWidth - sectionName.size(); i++ ) {
			cout << ".";
		}
		cout << " ";
	}

	~Block() {
		const float timeInMS = Tock();
		const float wholeMS = int( std::floor( timeInMS ) );
		const float partialMS = int( ( timeInMS - wholeMS ) * 1000.0f );
		cout << T_GREEN << "done." << T_RED << " ( " << std::setfill( ' ' ) << std::setw( 6 ) << wholeMS << "."
			<< std::setw( 3 ) << std::setfill( '0' ) << partialMS << TIMEUNIT << " )" << RESET << newline;
	}
};

void engine::StartMessage () {
	ZoneScoped;

	cout << endl << T_YELLOW << BOLD << "NQADE - Not Quite A Demo Engine" << newline;
	cout << " By Jon Baker ( 2020 - 2023 ) " << RESET << newline;
	cout << "  https://jbaker.graphics/ " << newline << newline;
}

void engine::LoadConfig () {
	ZoneScoped;

	{
		Block Start( "Configuring Application" );
		json j;
		// load the config json, populate config struct - this will probably have more data, eventually
		ifstream i( "src/engine/config.json" );
		i >> j; i.close();
		config.windowTitle = j[ "windowTitle" ];
		config.width = j[ "screenWidth" ];
		config.height = j[ "screenHeight" ];
		config.linearFilter = j[ "linearFilterDisplayTex" ];
		config.windowOffset.x = j[ "windowOffset" ][ "x" ];
		config.windowOffset.y = j[ "windowOffset" ][ "y" ];
		config.startOnScreen = j[ "startOnScreen" ];
		config.numMsDelayAfterCallback = j[ "numMsDelayAfterCallback" ];
		config.windowFlags |= ( j[ "SDL_WINDOW_FULLSCREEN" ] ? SDL_WINDOW_FULLSCREEN : 0 );
		config.windowFlags |= ( j[ "SDL_WINDOW_FULLSCREEN_DESKTOP" ] ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0 );
		config.windowFlags |= ( j[ "SDL_WINDOW_BORDERLESS" ] ? SDL_WINDOW_BORDERLESS : 0 );
		config.windowFlags |= ( j[ "SDL_WINDOW_RESIZABLE" ] ? SDL_WINDOW_RESIZABLE : 0 );
		config.windowFlags |= ( j[ "SDL_WINDOW_INPUT_GRABBED" ] ? SDL_WINDOW_INPUT_GRABBED : 0 );
		config.vSyncEnable = j[ "vSyncEnable" ];
		config.MSAACount = j[ "MSAACount" ];
		config.OpenGLVersionMajor = j[ "OpenGLVersionMajor" ];
		config.OpenGLVersionMinor = j[ "OpenGLVersionMinor" ];
		config.reportPlatformInfo = j[ "reportPlatformInfo" ];
		config.enableDepthTesting = j[ "enableDepthTesting" ];
		config.clearColor.r = j[ "clearColor" ][ "r" ];
		config.clearColor.g = j[ "clearColor" ][ "g" ];
		config.clearColor.b = j[ "clearColor" ][ "b" ];
		config.clearColor.a = j[ "clearColor" ][ "a" ];
		// color grading stuff
		tonemap.tonemapMode = j[ "colorGrade" ][ "tonemapMode" ];
		tonemap.gamma = j[ "colorGrade" ][ "gamma" ];
		tonemap.colorTemp = j[ "colorGrade" ][ "colorTemp" ];
	}
}

void engine::CreateWindowAndContext () {
	ZoneScoped;

	w.config = &config;

	{ Block Start( "Initializing SDL2" ); w.PreInit(); }
	{ Block Start( "Creating Window" ); w.Init(); }
	{ Block Start( "Setting Up OpenGL Context" ); w.OpenGLSetup(); }

	// setup OpenGL debug callback with configured delay
	numMsDelayAfterCallback = config.numMsDelayAfterCallback;
	GLDebugEnable();
	cout << endl << T_YELLOW << "  OpenGL debug callback enabled - " << numMsDelayAfterCallback << "ms delay.\n" << RESET << endl;
}

// split up into vertex, texture funcs + report platform info ( maybe do this later? )
void engine::DisplaySetup () {
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
}

void engine::SetupVertexData () {
	ZoneScoped;

	{	
		Block Start( "Setting Up Vertex Data" );

		// OpenGL core spec requires a VAO bound when calling glDrawArrays
		glGenVertexArrays( 1, &displayVAO );
		glBindVertexArray( displayVAO );

		// corresponding VBO, unused by the default NQADE template
		glGenBuffers( 1, &displayVBO );
		glBindBuffer( GL_ARRAY_BUFFER, displayVBO );

		// no op, by default...
			// load models, setup vertex attributes, etc, here
	}
}

void engine::SetupTextureData () {
	ZoneScoped;

	{
		Block Start( "Setting Up Textures" );

		GLuint accumulatorTexture;
		GLuint displayTexture;
		GLuint blueNoiseTexture;
		GLuint tridentImage;

		// create the image textures
		Image initial( config.width, config.height, false );
		glGenTextures( 1, &accumulatorTexture );
		glActiveTexture( GL_TEXTURE3 );
		glBindTexture( GL_TEXTURE_2D, accumulatorTexture );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, config.width, config.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &initial.data[ 0 ] );
		textures[ "Accumulator" ] = accumulatorTexture;

		glGenTextures( 1, &displayTexture );
		glActiveTexture( GL_TEXTURE0 );
		glBindTexture( GL_TEXTURE_2D, displayTexture );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, config.linearFilter ? GL_LINEAR : GL_NEAREST );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, config.linearFilter ? GL_LINEAR : GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, config.width, config.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &initial.data[ 0 ] );
		textures[ "Display Texture" ] = displayTexture;

		// blue noise image on the GPU
		Image blueNoiseImage{ "src/noise/blueNoise.png", LODEPNG };
		glGenTextures( 1, &blueNoiseTexture );
		glActiveTexture( GL_TEXTURE4 );
		glBindTexture( GL_TEXTURE_2D, blueNoiseTexture );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, blueNoiseImage.width, blueNoiseImage.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &blueNoiseImage.data[ 0 ] );
		textures[ "Blue Noise" ] = blueNoiseTexture;

		// create the image for the trident
		Image initialT( trident.blockDimensions.x * 8, trident.blockDimensions.y * 16 );
		glGenTextures( 1, &tridentImage );
		glActiveTexture( GL_TEXTURE5 );
		glBindTexture( GL_TEXTURE_2D, tridentImage );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, initialT.width, initialT.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, &initialT.data.data()[ 0 ] );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		trident.PassInImage( tridentImage );
		textures[ "Trident" ] = tridentImage;

		// add bayer textures from Voraldo13

	}

	{
		Block Start( "Setting Up Bindsets" );

		bindSets[ "Drawing" ] = bindSet( {
			binding( 0, textures[ "Blue Noise" ], GL_RGBA8UI ),
			binding( 1, textures[ "Accumulator" ], GL_RGBA8UI )
		} );

		bindSets[ "Postprocessing" ] = bindSet( {
			binding( 0, textures[ "Accumulator" ], GL_RGBA8UI ),
			binding( 1, textures[ "Display Texture" ], GL_RGBA8UI )
		} );

		bindSets[ "Display" ] = bindSet( {
			binding( 0, textures[ "Display Texture" ], GL_RGBA8UI )
		} );
	}
}

void engine::LoadData () {
	ZoneScoped;

	{
		Block Start( "Loading Palettes" );
		LoadPalettes();
	}

	{
		Block Start( "Loading Font Glyphs" );
		LoadGlyphs();
	}

	{
		Block Start( "Load Wordlists" );
		LoadBadWords();
		LoadColorWords();
	}
}

void engine::ShaderCompile () {
	ZoneScoped;

	{
		Block Start( "Compiling Shaders" );

		const string base( "./src/engine/shaders/" );

		// something to put some basic data in the accumulator texture
		shaders[ "Dummy Draw" ] = computeShader( base + "dummyDraw.cs.glsl" ).shaderHandle;

		// tonemapping shader
		shaders[ "Tonemap" ] = computeShader( base + "tonemap.cs.glsl" ).shaderHandle;

		// create the shader for the triangles to cover the screen
		shaders[ "Display" ] = regularShader( base + "blit.vs.glsl", base + "blit.fs.glsl" ).shaderHandle;

		// initialize the text renderer
		shaders[ "Font Renderer" ] = computeShader( "src/fonts/fontRenderer/font.cs.glsl" ).shaderHandle;
		textRenderer.Init( config.width, config.height, shaders[ "Font Renderer" ] );

		// trident shaders
		shaders[ "Trident Raymarch" ] = computeShader( "./src/trident/tridentGenerate.cs.glsl" ).shaderHandle;
		shaders[ "Trident Blit" ] = computeShader( "./src/trident/tridentCopy.cs.glsl" ).shaderHandle;
		trident.basePt = textRenderer.basePt; // location to draw at
		trident.PassInShaders( shaders[ "Trident Raymarch" ], shaders[ "Trident Blit" ] );
	}
}

void engine::ImguiSetup () {
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
		ImGui_ImplSDL2_InitForOpenGL( w.window, w.GLcontext );
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

void engine::InitialClear () {
	ZoneScoped;

	{
		Block Start( "Clear Buffer" );

		glClearColor( config.clearColor.x, config.clearColor.y, config.clearColor.z, config.clearColor.w );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		w.Swap();
	}
}

void engine::ReportStartupStats () {
	ZoneScoped;

	cout << endl << T_CYAN << "  " << shaders.size() << " shader programs, " << textures.size() << " textures, " << bindSets.size() << " bindsets" << endl;
	cout << T_YELLOW << "  Startup is complete ( total " << TotalTime() << TIMEUNIT << " )" << RESET << endl << endl;
}

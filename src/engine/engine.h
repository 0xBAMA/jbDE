#ifndef ENGINE
#define ENGINE
#include "includes.h"

class engine {
protected:
//====== Application Handles and Basic Data ===================================
	windowHandler window;		// OpenGL context and SDL2 window
	configData config;			// loaded from config.json
	layerManager textRenderer;	// text renderer framework
	orientTrident trident;		// orientation gizmo from Voraldo13

//====== OpenGL ===============================================================
	GLuint displayVAO;
	GLuint displayVBO;

	// resource management
	unordered_map< string, GLuint > textures;
	unordered_map< string, GLuint > shaders;
	unordered_map< string, bindSet > bindSets;

//====== Tonemapping Parameters + Adjustment ==================================
	colorGradeParameters tonemap;
	void TonemapControlsWindow ();
	void SendTonemappingParameters ();

//====== Initialization =======================================================
	void StartBlock ( string sectionName );
	void EndBlock ();
	void Init ();
	void PostInit ();
	void StartMessage ();
	void LoadConfig ();
	void CreateWindowAndContext ();
	void DisplaySetup ();
	void SetupVertexData ();
	void SetupTextureData ();
	void ShaderCompile ();
	void LoadData ();
	void ImguiSetup ();
	void InitialClear ();
	void ReportStartupStats ();

//====== Main Loop Functions ==================================================
	void BlitToScreen ();
	void HandleQuitEvents ();
	void HandleTridentEvents ();
	void ClearColorAndDepth ();
	void DrawAPIGeometry ();
	void ComputePasses ();
	void ImguiPass ();
	void ImguiFrameStart ();
	void ImguiFrameEnd ();
	void DrawTextEditor ();
	void MenuLayout ( bool* open );
	void QuitConf ( bool* open );

//====== ImGui Windows ========================================================
	bool showDemoWindow;

//====== LegitProfiler Data ===================================================
	void PrepareProfilingData ();
	std::vector< legit::ProfilerTask > tasks_CPU;
	std::vector< legit::ProfilerTask > tasks_GPU;

//====== Shutdown Procedures ==================================================
	void ImguiQuit ();
	void Quit ();

//====== Program Flags ========================================================
	bool quitConfirm = false;
	bool pQuit = false;
};

// placeholder - will return to this soon, once the child virtual funcs are doing something non-trivial
class engineChild : public engine {
public:
	engineChild () { Init(); OnInit(); PostInit(); }
	~engineChild () { Quit(); }

	bool MainLoop () {
		ZoneScoped;

		HandleTridentEvents();		// event handling - need to add custom event handling for the derived class
		HandleQuitEvents();

		ClearColorAndDepth();		// if I just disable depth testing, this can disappear
		DrawAPIGeometry();			// draw any API geometry desired
		ComputePasses();			// multistage update of displayTexture
		BlitToScreen();				// fullscreen triangle copying to the screen
		ImguiPass();				// do all the gui stuff - this needs to be broken out into more pieces
		window.Swap();				// show what has just been drawn to the back buffer
		FrameMark;					// tells tracy that this is the end of a frame
		PrepareProfilingData();		// get profiling data ready for next frame
		return pQuit;
	}

	void OnInit () {
		{ Block Start( "Additional User Init" ); // currently this is mostly for feature testing in oneShot mode
			// rng gen( 0, 500, 42069 );
			// Image_4U histogram( 500, 500 );
			// for ( uint32_t i{ 0 }; i < 100000; i++ ) {
			// 	int val = int( gen() );
			// 	for ( uint32_t y{ 0 }; y < histogram.Height(); y++ ) {
			// 		color_4U read = histogram.GetAtXY( val, y );
			// 		if ( read[ alpha ] == 0 ) {
			// 			read[ red ] = read[ green ] = read[ blue ] = read[ alpha ] = 255 * ( i / 100000.0f );
			// 			histogram.SetAtXY( val, y, read );
			// 			break;
			// 		}
			// 	}
			// }
			// histogram.FlipVertical();
			// histogram.Save( "histogram.png" );
		}
	}

	void OnUpdate () {

	}

	void OnRender () {
		
	}
};

#endif

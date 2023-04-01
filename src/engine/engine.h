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


class engineChild : public engine {	// example derived class
public:
	engineChild () { Init(); OnInit(); PostInit(); }
	~engineChild () { Quit(); }

	bool MainLoop () {				// this is what's called from the loop in main
		ZoneScoped;

		HandleTridentEvents();		// event handling
		HandleCustomEvents();
		HandleQuitEvents();

		// derived-class-specific functionality
		OnUpdate();
		OnRender();

		FrameMark;					// tells tracy that this is the end of a frame
		PrepareProfilingData();		// get profiling data ready for next frame
		return pQuit;
	}

	void HandleCustomEvents () {
		// application specific controls
	}

	void OnInit () {
		{ Block Start( "Additional User Init" );
			// currently this is mostly for feature testing in oneShot mode

			// this will also contain application specific textures, shaders, and bindsets
		}
	}

	void OnUpdate () {
		// application-specific update code
	}

	void OnRender () {
		ClearColorAndDepth();		// if I just disable depth testing, this can disappear
		DrawAPIGeometry();			// draw any API geometry desired
		ComputePasses();			// multistage update of displayTexture
		BlitToScreen();				// fullscreen triangle copying to the screen
		ImguiPass();				// do all the gui stuff - this needs to be broken out into more pieces
		window.Swap();				// show what has just been drawn to the back buffer
	}
};

#endif

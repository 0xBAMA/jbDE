#ifndef ENGINE
#define ENGINE
#include "includes.h"

class engineBase {
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

public:
	timerManager timerQueries_engine;

//====== Shutdown Procedures ==================================================
protected:
	void ImguiQuit ();
	void Quit ();

//====== Program Flags ========================================================
	bool quitConfirm = false;
	bool pQuit = false;

};

#endif

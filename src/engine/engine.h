#ifndef ENGINE
#define ENGINE
#include "includes.h"

class engine {
public:
//====== Public Interface =====================================================
	engine()  { Init(); }
	~engine() { Quit(); }

	bool MainLoop (); // called from loop in main

protected:
//====== Application Handles and Basic Data ===================================
	windowHandler w;			// OpenGL context and SDL2 window
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
	void HandleEvents ();
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

//====== User Provided Functions ==============================================
// this needs work, but this is where application-specific work will take place
	// virtual void OnInit() = 0;
	// virtual void OnUpdate() = 0;
	// virtual void OnRender() = 0;

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
	// virtual void OnInit() override {

	// 	// not being called - investigate

	// 	cout << "there are " << paletteList.size() << " palettes" << newline;
	// 	// Image i (  );

	// }

	// virtual void OnUpdate() override {}
	// virtual void OnRender() override {}
};

#endif

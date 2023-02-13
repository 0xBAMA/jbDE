#ifndef ENGINE
#define ENGINE
#include "includes.h"

class engine {
public:
//====== Public Interface =====================================================
	engine()  { Init(); }
	~engine() { Quit(); }

	bool MainLoop (); // called from loop in main

private:
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

//====== tonemapping parameters + adjustment ==================================
	colorGradeParameters tonemap;
	void TonemapControlsWindow ();
	void SendTonemappingParameters ();

//====== initialization =======================================================
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

//====== main loop functions ==================================================
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

//====== shutdown procedures ==================================================
	void ImguiQuit ();
	void Quit ();

//====== program flags ========================================================
	bool quitConfirm = false;
	bool pQuit = false;

};
#endif

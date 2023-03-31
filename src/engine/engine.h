#ifndef ENGINE
#define ENGINE
#include "includes.h"

class engine {
public:
//====== Public Interface =====================================================
	// engine()  { Init(); }
	// ~engine() { Quit(); }
	// bool MainLoop (); // called from loop in main

	engine() {}
	~engine() {}
	bool MainLoop ();

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
	engineChild() { Init(); OnInit(); PostInit(); }
	~engineChild() { Quit(); }

	void OnInit() {
		{ Block Start( "Additional User Init" );
		// currently this is mostly for feature testing in oneShot mode

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
};

#endif

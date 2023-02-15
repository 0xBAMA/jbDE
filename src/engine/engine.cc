#include "engine.h"

// initialization of OpenGL, etc
void engine::Init () {
	ZoneScoped;
	StartMessage();
	LoadConfig();
	CreateWindowAndContext();
	DisplaySetup();
	SetupVertexData();
	SetupTextureData();
	ShaderCompile();
	LoadData();
	ImguiSetup();
	InitialClear();
	if ( !config.oneShot ) // if init takes some time, don't show the window before it's done - also oneShot does not need to ever show the window
		SDL_ShowWindow( w.window );
	ReportStartupStats();
}

// terminate ImGUI
void engine::ImguiQuit () {
	ZoneScoped;
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

// function to terminate, called from destructor
void engine::Quit () {
	ZoneScoped;
	ImguiQuit();
	w.Kill();
}

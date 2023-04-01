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
	ImguiSetup();
	LoadData();
}

// after the derived class custom init
void engine::PostInit () {
	InitialClear();
	timerQueries = &timerQueries_engine;
	window.ShowIfNotHeadless();
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
	window.Kill();
}

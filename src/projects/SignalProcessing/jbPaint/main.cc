#include "../../../engine/engine.h"

struct jbPaintState {
	float brushRadius = 25.0f;
	float brushSlope = 10.0f;
	float brushThreshold = 0.4f;
	vec3 brushColor = vec3( 0.1f );
};

class jbPaint final : public engineBase { // sample derived from base engine class
public:
	jbPaint () { Init(); OnInit(); PostInit(); }
	~jbPaint () { Quit(); }

	jbPaintState state;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture
			shaders[ "Draw" ] = computeShader( "./src/projects/SignalProcessing/jbPaint/shaders/draw.cs.glsl" ).shaderHandle;
			shaders[ "Update" ] = computeShader( "./src/projects/SignalProcessing/jbPaint/shaders/update.cs.glsl" ).shaderHandle;

			// paintBuffer
			textureOptions_t opts;
			opts.dataType		= GL_RGBA16F;
			opts.width			= config.width;
			opts.height			= config.height;
			opts.textureType	= GL_TEXTURE_2D;
			textureManager.Add( "Paint Buffer", opts );

		}
	}

	void ImguiPass () {
		ZoneScoped;
		if ( tonemap.showTonemapWindow ) {
			TonemapControlsWindow();
		}

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		{
			ImGui::Begin( "Controls" );
			ImGui::SeparatorText( "Brush" );
			ImGui::SliderFloat( "Radius", &state.brushRadius, 0.001f, 100.0f );
			ImGui::SliderFloat( "Threshold", &state.brushThreshold, 0.0f, 1.0f );
			ImGui::SliderFloat( "Slope", &state.brushSlope, 0.0f, 100.0f );
			ImGui::ColorEdit3( "Color", ( float * ) &state.brushColor, ImGuiColorEditFlags_PickerHueWheel );
			ImGui::Separator();
		}

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();

			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			textureManager.BindImageForShader( "Paint Buffer", "paintBuffer", shader, 2 );

			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // postprocessing - shader for color grading ( color temp, contrast, gamma ... ) + tonemapping
			scopedTimer Start( "Postprocess" );
			bindSets[ "Postprocessing" ].apply();
			glUseProgram( shaders[ "Tonemap" ] );
			SendTonemappingParameters();
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );
			textRenderer.Clear();
			textRenderer.Update( ImGui::GetIO().DeltaTime );

			// show terminal, if active - check happens inside
			textRenderer.drawTerminal( terminal );

			// put the result on the display
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		const GLuint shader = shaders[ "Update" ];

		glUseProgram( shader );

		const bool mouseState = inputHandler.getState( MOUSE_BUTTON_LEFT ) && !ImGui::GetIO().WantCaptureMouse;

		static rngi noiseOffset = rngi( 0, 512 );
		glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), noiseOffset(), noiseOffset() );

		ivec2 mousePos = inputHandler.getMousePos();
		glUniform3i( glGetUniformLocation( shader, "mouseState" ), mousePos.x, mousePos.y, mouseState );

		glUniform1f( glGetUniformLocation( shader, "brushRadius" ), state.brushRadius );
		glUniform1f( glGetUniformLocation( shader, "brushSlope" ), state.brushSlope );
		glUniform1f( glGetUniformLocation( shader, "brushThreshold" ), state.brushThreshold );
		glUniform3f( glGetUniformLocation( shader, "brushColor" ), state.brushColor.r, state.brushColor.g, state.brushColor.b );

		textureManager.BindImageForShader( "Blue Noise", "blueNoiseTexture", shader, 0 );
		textureManager.BindImageForShader( "Paint Buffer", "paintBuffer", shader, 2 );

		glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}

	void OnRender () {
		ZoneScoped;
		ClearColorAndDepth();		// if I just disable depth testing, this can disappear
		ComputePasses();			// multistage update of displayTexture
		BlitToScreen();				// fullscreen triangle copying to the screen
		{
			scopedTimer Start( "ImGUI Pass" );
			ImguiFrameStart();		// start the imgui frame
			ImguiPass();			// do all the gui stuff
			ImguiFrameEnd();		// finish imgui frame and put it in the framebuffer
		}
		window.Swap();				// show what has just been drawn to the back buffer
	}

	bool MainLoop () { // this is what's called from the loop in main
		ZoneScoped;

		// get new data into the input handler
		inputHandler.update();

		// pass any signals into the terminal (active check happens inside)
		terminal.update( inputHandler );

		// event handling
		HandleQuitEvents();

		// derived-class-specific functionality
		OnUpdate();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	jbPaint engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

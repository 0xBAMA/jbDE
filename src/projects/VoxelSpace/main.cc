#include "../../engine/engine.h"

struct VoxelSpaceConfig_t {

	// what sim do we want to run
	int32_t mode;

	// renderer config
	vec2 viewPosition	= vec2( 512, 512 );
	int viewerHeight	= 75;
	float viewAngle		= -0.425f;
	float maxDistance	= 800.0f;
	int horizonLine		= 700;
	float heightScalar	= 1200.0f;
	float offsetScalar	= 0.0f;
	float fogScalar		= 0.451f;
	float stepIncrement	= 0.0f;
	float FoVScalar		= 0.85f;
	float viewBump		= 275.0f;
	float minimapScalar	= 0.3f;
	bool adaptiveHeight	= false;

	// thread sync, erosion control
	bool threadShouldRun;
	bool erosionReady;

};

class VoxelSpace : public engineBase {	// example derived class
public:
	VoxelSpace () { Init(); OnInit(); PostInit(); }
	~VoxelSpace () { Quit(); }

	VoxelSpaceConfig_t voxelSpaceConfig;
	GLuint renderFramebuffer;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// load config
			json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();
			voxelSpaceConfig.mode = j[ "app" ][ "VoxelSpace" ][ "mode" ];

			// compile all the shaders

			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Background" ] = computeShader( "./src/projects/VoxelSpace/shaders/Background.cs.glsl" ).shaderHandle;
			shaders[ "VoxelSpace" ] = computeShader( "./src/projects/VoxelSpace/shaders/VoxelSpace.cs.glsl" ).shaderHandle;
			// todo: fullscreen triangle shader, less specialized than blit

			// create a framebuffer to accumulate the images
				// 16-bit color target gets fed into the tonemapping step
			textureOptions_t opts;
			opts.width = config.width;
			opts.height = config.height;
			opts.dataType = GL_RGBA16F;
			opts.textureType = GL_TEXTURE_2D;
			textureManager.Add( "Framebuffer Color", opts );
			opts.dataType = GL_DEPTH_COMPONENT16;
			textureManager.Add( "Framebuffer Depth", opts );

			// // creating the actual framebuffers with their attachments
			const GLenum bufs[] = { GL_COLOR_ATTACHMENT0 }; // framebuffer just has the one color attachment + depth
			glGenFramebuffers( 1, &renderFramebuffer );
			glBindFramebuffer( GL_FRAMEBUFFER, renderFramebuffer );
			glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureManager.Get( "Framebuffer Depth" ), 0 );
			glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureManager.Get( "Framebuffer Color" ), 0 );
			glDrawBuffers( 1, bufs ); // how many active attachments, and their attachment locations
			if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
				cout << newline << "framebuffer creation successful" << endl;
			}

			// create the image to draw the regular map into

			// create the image to draw the minimap into



			// create the texture for the landscape
			if ( voxelSpaceConfig.mode == 0 ) {
				// we know that we want to run the live erosion sim

				// create the initial heightmap that's used for the erosion

				// create the texture from that heightmap
				textureOptions_t opts;
				opts.width = config.width;
				opts.height = config.height;
				opts.textureType = GL_TEXTURE_2D;
				// ...

				textureManager.Add( "Map", opts );

				// set threadShouldRun flag, since once everything is configured, the thread should run
				voxelSpaceConfig.threadShouldRun = true;
				// unset erosionReady flag, since that data is now potentially in flux
				voxelSpaceConfig.erosionReady = false;

			} else if ( voxelSpaceConfig.mode >= 1 && voxelSpaceConfig.mode <= 30 ) {
				// we want to load one of the basic maps from disk - color in the rgb + height in alpha
				Image_4U map( string( "./src/projects/VoxelSpace/data/map" ) + std::to_string( voxelSpaceConfig.mode ) + string( ".png" ) );

				// create the texture from the loaded image
				textureOptions_t opts;
				opts.width = map.Width();
				opts.height = map.Height();
				opts.dataType = GL_RGBA8;
				opts.textureType = GL_TEXTURE_2D;
				opts.pixelDataType = GL_UNSIGNED_BYTE;
				opts.initialData = ( void * ) map.GetImageDataBasePtr();

				textureManager.Add( "Map", opts );

			} else {
				cout << "invalid mode selected" << newline;
				abort();
			}
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );
		// const uint8_t * state = SDL_GetKeyboardState( NULL );
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

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void DrawAPIGeometry () {
		ZoneScoped; scopedTimer Start( "API Geometry" );
		// draw some shit
	}

	void ComputePasses () {
		ZoneScoped;

		{
			if ( voxelSpaceConfig.mode == -1 ) {
				// check to see if the erosion thread is ready
					// if it is
						// convert to a uint version
						// pass it to the image
						// run the "shade" shader

				// barrier
			}

			// update the main rendered view
			glUseProgram( shaders[ "VoxelSpace" ] );
			textureManager.BindTexForShader( "Map", "map", shaders[ "VoxelSpace" ], 3 );
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

			// update the minimap
			// ...
		}

		// bind the framebuffer
		// glBindFramebuffer( GL_FRAMEBUFFER, resources.FBOs[ "Geometry" ] );

			// clear to the background color

		// fullscreen triangle passes:

			// draw the main rendered view, blending with the background color

			// draw the minimap view, blending with the existing contents

		// this color target becomes the input for the postprocessing step

		{ // postprocessing - shader for color grading ( color temp, contrast, gamma ... ) + tonemapping
			scopedTimer Start( "Postprocess" );
			// bindSets[ "Postprocessing" ].apply();
			glUseProgram( shaders[ "Tonemap" ] );
			SendTonemappingParameters();
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );
			textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		// application-specific update code
	}

	void OnRender () {
		ZoneScoped;
		ClearColorAndDepth();		// if I just disable depth testing, this can disappear
		DrawAPIGeometry();			// draw any API geometry desired
		ComputePasses();			// multistage update of displayTexture
		// BlitToScreen();				// fullscreen triangle copying to the screen
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

		// event handling
		HandleCustomEvents();
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
	VoxelSpace VoxelSpaceInstance;
	while( !VoxelSpaceInstance.MainLoop() );
	return 0;
}

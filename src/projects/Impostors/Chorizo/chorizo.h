#include "../../../engine/engine.h"
#include "generate.h"

struct ChorizoConfig_t {
	GLuint vao;
	GLuint transformBuffer;
	GLuint primaryFramebuffer[ 2 ];

	int numSpheres = 500;
	uint32_t frameCount = 0;
};

class Chorizo : public engineBase {
public:
	Chorizo () { Init(); OnInit(); PostInit(); }
	~Chorizo () { Quit(); }

	ChorizoConfig_t ChorizoConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Dummy Draw" ] = computeShader( "./src/projects/Impostors/Chorizo/shaders/dummyDraw.cs.glsl" ).shaderHandle;

			// setup raster shader
			shaders[ "BBox" ] = regularShader(
				"./src/projects/Impostors/Chorizo/shaders/bbox.vs.glsl",
				"./src/projects/Impostors/Chorizo/shaders/bbox.fs.glsl"
			).shaderHandle;

			// create and bind a basic vertex array
			glCreateVertexArrays( 1, &ChorizoConfig.vao );
			glBindVertexArray( ChorizoConfig.vao );

			// single sided polys - only draw those which are facing outwards
			glEnable( GL_CULL_FACE );
			glCullFace( GL_BACK );
			glFrontFace( GL_CCW );
			glDisable( GL_BLEND );

			// create the transforms buffer
			std::vector< mat4 > transforms;
			// recursiveTreeBuild( transforms, glm::scale( vec3( 0.1f ) ) * mat4( 1.0f ), 0, 12 );
			randomPlacement( transforms, 500000 );
			ChorizoConfig.numSpheres = transforms.size() / 2;
			cout << newline << newline << "created " << ChorizoConfig.numSpheres / 2 << " primitives" << newline;

			glGenBuffers( 1, &ChorizoConfig.transformBuffer );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, ChorizoConfig.transformBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( mat4 ) * ChorizoConfig.numSpheres * 2, ( GLvoid * ) transforms.data(), GL_DYNAMIC_COPY );
			// glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( mat4 ) * ChorizoConfig.numSpheres * 2, ( GLvoid * ) transforms.data(), GL_STATIC_DRAW );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, ChorizoConfig.transformBuffer );

			// == Render Framebuffer(s) ===========
			textureOptions_t opts;
			// ==== Depth =========================
			opts.dataType = GL_DEPTH_COMPONENT32;
			opts.textureType = GL_TEXTURE_2D;
			opts.width = config.width;
			opts.height = config.height;
			textureManager.Add( "Framebuffer Depth 0", opts );
			textureManager.Add( "Framebuffer Depth 1", opts );
			// ==== Normal =======================
			opts.dataType = GL_RGBA16F;
			textureManager.Add( "Framebuffer Normal 0", opts );
			textureManager.Add( "Framebuffer Normal 1", opts );
			// ==== Primitive ID ==================
			opts.dataType = GL_RG32UI;
			textureManager.Add( "Framebuffer Primitive ID 0", opts );
			textureManager.Add( "Framebuffer Primitive ID 1", opts );
			// ====================================

			// setup the buffers for the rendering process
			glGenFramebuffers( 2, &ChorizoConfig.primaryFramebuffer[ 0 ] );

			// both framebuffers have implicit depth + 2 color attachments
			const GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };

			// creating the actual framebuffers with their attachments
			glBindFramebuffer( GL_FRAMEBUFFER, ChorizoConfig.primaryFramebuffer[ 0 ] );
			glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureManager.Get( "Framebuffer Depth 0" ), 0 );
			glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureManager.Get( "Framebuffer Normal 0" ), 0 );
			glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, textureManager.Get( "Framebuffer Primitive ID 0" ), 0 );
			glDrawBuffers( 2, bufs ); // how many active attachments, and their attachment locations
			if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
				cout << newline << "front framebuffer creation successful" << newline;
			}

			glBindFramebuffer( GL_FRAMEBUFFER, ChorizoConfig.primaryFramebuffer[ 1 ] );
			glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureManager.Get( "Framebuffer Depth 1" ), 0 );
			glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureManager.Get( "Framebuffer Normal 1" ), 0 );
			glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, textureManager.Get( "Framebuffer Primitive ID 1" ), 0 );
			glDrawBuffers( 2, bufs );
			if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
				cout << "back framebuffer creation successful" << newline << newline;
			}

			// bind default framebuffer
			glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// // current state of the whole keyboard
		// const uint8_t * state = SDL_GetKeyboardState( NULL );

		// // current state of the modifier keys
		// const SDL_Keymod k	= SDL_GetModState();
		// const bool shift		= ( k & KMOD_SHIFT );
		// const bool alt		= ( k & KMOD_ALT );
		// const bool control	= ( k & KMOD_CTRL );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

	}

	void ImguiPass () {
		ZoneScoped;

		// if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
		// if ( tonemap.showTonemapWindow ) TonemapControlsWindow();

		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void DrawAPIGeometry () {
		ZoneScoped; scopedTimer Start( "API Geometry" );

		ChorizoConfig.frameCount++;

		glBindFramebuffer( GL_FRAMEBUFFER, ChorizoConfig.primaryFramebuffer[ ( ChorizoConfig.frameCount % 2 ) ] );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

		const GLuint shader = shaders[ "BBox" ];
		glUseProgram( shader );
		glBindVertexArray( ChorizoConfig.vao );

		const float aspectRatio = float( config.width ) / float( config.height );
		// const vec3 eyePosition = vec3( 2.0f, 0.0f, 0.0f );
		const float time = SDL_GetTicks() / 10000.0f;
		const vec3 eyePosition = vec3( 5.0f * sin( time ), 5.0f * cos( time * 0.2f ), 2.0f * sin( time * 0.3f ) );
		const mat4 viewTransform =
			glm::perspective( 45.0f, aspectRatio, 0.001f, 100.0f ) *
			glm::lookAt( eyePosition, vec3( 0.0f ), vec3( 0.0f, 1.0f, 0.0f ) );

		glUniform1f( glGetUniformLocation( shader, "numPrimitives" ), ChorizoConfig.numSpheres );
		glUniformMatrix4fv( glGetUniformLocation( shader, "viewTransform" ), 1, GL_FALSE, glm::value_ptr( viewTransform ) );
		glUniform3fv( glGetUniformLocation( shader, "eyePosition" ), 1, glm::value_ptr( eyePosition ) );

		glViewport( 0, 0, config.width, config.height );
		glDrawArrays( GL_TRIANGLES, 0, 36 * ChorizoConfig.numSpheres );

		// revert to default framebuffer
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Dummy Draw" ] );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "time" ), SDL_GetTicks() / 1600.0f );
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		// add the code for deferred shading, etc, here

		{ // postprocessing - shader for color grading ( color temp, contrast, gamma ... ) + tonemapping
			scopedTimer Start( "Postprocess" );
			bindSets[ "Postprocessing" ].apply();
			glUseProgram( shaders[ "Tonemap" ] );
			SendTonemappingParameters();
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		// shader to apply dithering
			// ...

		// other postprocessing
			// ...

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );
			textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // show trident with current orientation
			// scopedTimer Start( "Trident" );
			// trident.Update( textureManager.Get( "Display Texture" ) );
			// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		// application-specific update code
	}

	void OnRender () {
		ZoneScoped;
		ClearColorAndDepth();		// if I just disable depth testing, this can disappear
		ComputePasses();			// multistage update of displayTexture
		BlitToScreen();				// fullscreen triangle copying to the screen
		DrawAPIGeometry();			// draw the cubes on top
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
		HandleTridentEvents();
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

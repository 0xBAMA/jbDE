#include "../../../engine/engine.h"
#include "generate.h"

struct ChorizoConfig_t {
	GLuint vao;
	GLuint shapeParametersBuffer;
	GLuint boundsTransformBuffer;
	GLuint primaryFramebuffer[ 2 ];

	vec3 eyePosition;
	mat4 viewTransform;
	mat4 viewTransformInverse;
	mat4 projTransform;
	mat4 projTransformInverse;
	mat4 combinedTransform;
	mat4 combinedTransformInverse;

	geometryManager_t geometryManager;
	int numPrimitives = 0;
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
			shaders[ "Draw" ] = computeShader( "./src/projects/Impostors/Chorizo/shaders/draw.cs.glsl" ).shaderHandle;
			shaders[ "Bounds" ] = computeShader( "./src/projects/Impostors/Chorizo/shaders/bounds.cs.glsl" ).shaderHandle;
			shaders[ "Animate" ] = computeShader( "./src/projects/Impostors/Chorizo/shaders/animate.cs.glsl" ).shaderHandle;

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

			// get some initial data for the impostor shapes
			PrepSSBOs();

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

			// setup the buffers for the rendering process - front and back framebuffers
			glGenFramebuffers( 2, &ChorizoConfig.primaryFramebuffer[ 0 ] );
			const GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 }; // both framebuffers have implicit depth + 2 color attachments

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

	void PrepSSBOs () {

		// add some number of primitives to the buffer
		// rng pick = rng( 0.0f, 1.0f );
		const float xSpan = 1.6f;
		const float xSpacing = 0.2f;
		const float ySpan = 0.8f;
		const float ySpacing = 0.2f;
		// for ( float x = -xSpan; x <= xSpan; x += xSpacing ) {
		// 	ChorizoConfig.geometryManager.AddCapsule( vec3( x, -ySpan, 0.0f ), vec3( x, ySpan, 0.0f ), 0.03f );
		// }
		// for ( float y = -ySpan; y <= ySpan; y += ySpacing ) {
		// 	ChorizoConfig.geometryManager.AddCapsule( vec3( -xSpan, y, 0.0f ), vec3( xSpan, y, 0.0f ), 0.03f );
		// }

		for ( float x = -xSpan; x <= xSpan; x += xSpacing ) {
			for ( float y = -ySpan; y <= ySpan; y += ySpacing ) {
				ChorizoConfig.geometryManager.AddCapsule( vec3( x, y, 0.0f ), vec3( x + xSpacing, y, 0.0f ), 0.03f );
				ChorizoConfig.geometryManager.AddCapsule( vec3( x, y, 0.0f ), vec3( x, y + ySpacing, 0.0f ), 0.03f );
			}
		}

		ChorizoConfig.numPrimitives = ChorizoConfig.geometryManager.count;
		cout << newline << "Created " << ChorizoConfig.numPrimitives << " primitives" << newline;

		// create the transforms buffer
		glGenBuffers( 1, &ChorizoConfig.boundsTransformBuffer );
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ChorizoConfig.boundsTransformBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( mat4 ) * ChorizoConfig.numPrimitives, NULL, GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, ChorizoConfig.boundsTransformBuffer );

		// shape parameterization buffer
		glGenBuffers( 1, &ChorizoConfig.shapeParametersBuffer );
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ChorizoConfig.shapeParametersBuffer );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( vec4 ) * ChorizoConfig.numPrimitives * 4, ( GLvoid * ) ChorizoConfig.geometryManager.parametersList.data(), GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, ChorizoConfig.shapeParametersBuffer );

	}

	void PrepSceneParameters () {
		const float time = SDL_GetTicks() / 3000.0f;

		// setting up the orbiting camera
		static rng ampGen = rng( 2.0f, 4.0f );
		static rng orbitGen = rng( 0.5f, 1.6f );
		static const vec3 amplitudes = vec3( ampGen(), ampGen(), ampGen() );
		static const vec3 orbitRatios = vec3( orbitGen(), orbitGen(), orbitGen() );
		ChorizoConfig.eyePosition = vec3(
			amplitudes.x * sin( time * orbitRatios.x ),
			amplitudes.y * cos( time * orbitRatios.y ),
			amplitudes.z * sin( time * orbitRatios.z ) );

		const float aspectRatio = float( config.width ) / float( config.height );

		ChorizoConfig.projTransform = glm::perspective( 45.0f, aspectRatio, 0.1f, 100.0f );
		ChorizoConfig.projTransformInverse = glm::inverse( ChorizoConfig.projTransform );

		ChorizoConfig.viewTransform = glm::lookAt( ChorizoConfig.eyePosition, vec3( 0.0f ), vec3( 0.0f, 1.0f, 0.0f ) );
		ChorizoConfig.viewTransformInverse = glm::inverse( ChorizoConfig.viewTransform );

		ChorizoConfig.combinedTransform = ChorizoConfig.projTransform * ChorizoConfig.viewTransform;
		ChorizoConfig.combinedTransformInverse = glm::inverse( ChorizoConfig.combinedTransform );

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

		// prepare the bounding boxes
		const GLuint shader = shaders[ "Bounds" ];
		glUseProgram( shader );
		glDispatchCompute( ( ChorizoConfig.numPrimitives + 63 ) / 64, 1, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		// then draw, using the prepared data
		RasterGeoDataSetup();
		glBindFramebuffer( GL_FRAMEBUFFER, ChorizoConfig.primaryFramebuffer[ ( ChorizoConfig.frameCount++ % 2 ) ] );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
		glViewport( 0, 0, config.width, config.height );
		glDrawArrays( GL_TRIANGLES, 0, 36 * ChorizoConfig.numPrimitives );

		// revert to default framebuffer
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}

	void RasterGeoDataSetup () {
		const GLuint shader = shaders[ "BBox" ];
		glUseProgram( shader );
		glBindVertexArray( ChorizoConfig.vao );

		glUniform1f( glGetUniformLocation( shader, "numPrimitives" ), ChorizoConfig.numPrimitives );
		glUniformMatrix4fv( glGetUniformLocation( shader, "viewTransform" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.combinedTransform ) );
		glUniform3fv( glGetUniformLocation( shader, "eyePosition" ), 1, glm::value_ptr( ChorizoConfig.eyePosition ) );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // doing the deferred processing from rasterizer results to rendered result
			scopedTimer Start( "Drawing" );
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );

			string index = ChorizoConfig.frameCount % 2 ? "0" : "1";
			string indexBack = ChorizoConfig.frameCount % 2 ? "1" : "0";
			textureManager.BindImageForShader( "Blue Noise", "blueNoiseTexture", shader, 0 );
			textureManager.BindImageForShader( "Framebuffer Normal " + index, "normals", shader, 1 );
			textureManager.BindImageForShader( "Framebuffer Normal " + indexBack, "normalsBack", shader, 2 );
			textureManager.BindImageForShader( "Framebuffer Primitive ID " + index, "primitiveID", shader, 3 );
			textureManager.BindImageForShader( "Framebuffer Primitive ID " + indexBack, "primitiveIDBack", shader, 4 );
			textureManager.BindImageForShader( "Accumulator", "accumulatorTexture", shader, 5 );

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
			textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );
		const GLuint shader = shaders[ "Animate" ];
		glUseProgram( shader );
		glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 3000.0f );
		glDispatchCompute( ( ChorizoConfig.numPrimitives + 63 ) / 64, 1, 1 );
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}

	void OnRender () {
		ZoneScoped;
		PrepSceneParameters();
		ClearColorAndDepth();		// if I just disable depth testing, this can disappear
		DrawAPIGeometry();			// draw the cubes on top, into the ping-ponging framebuffer
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

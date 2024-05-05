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

	float nearZ = 0.1f;
	float farZ = 100.0f;

	float FoV = 45.0f;

	float ringPosition = 0.0f;
	float blendAmount = 0.1f;
	float railAdjust = -0.3f;
	float apertureAdjust = 0.01f;

	rngi wangSeeder = rngi( 0, 10042069 );

	geometryManager_t geometryManager;
	int numPrimitives = 0;
	uint32_t frameCount = 0;
};

class Chorizo final : public engineBase {
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

	struct recursiveTreeConfig {
		int numBranches = 3;
		float branchTilt = 0.1f;
		float branchLength = 0.6f;
		float branchRadius = 0.04f;
		float lengthShrink = 0.8f;
		float radiusShrink = 0.79f;
		float paletteJitter = 0.03f;
		int levelsDeep = 0;
		int maxLevels = 11;
		vec3 basePoint = vec3( 0.0f, 0.0f, -0.5f );
		mat3 basis = mat3( 1.0f );
	};

	rng paletteJitter = rng( -1.0f, 1.0f );

	void TreeRecurse ( recursiveTreeConfig config ) {
		vec3 basePointNext = config.basePoint + config.branchLength * config.basis * vec3( 0.0f, 0.0f, 1.0f );
		vec3 color = palette::paletteRef( float( config.levelsDeep ) / float( config.maxLevels ) + paletteJitter() * config.paletteJitter );
		ChorizoConfig.geometryManager.AddCapsule( config.basePoint, basePointNext, config.branchRadius, color );
		config.basePoint = basePointNext;
		if ( config.levelsDeep == config.maxLevels ) {
			return;
		} else {
			rng jitter = rng( 0.0f, 0.5f );
			rng jitter2 = rng( 0.8f, 1.1f );
			config.levelsDeep++;
			config.branchRadius = config.branchRadius * ( config.radiusShrink * jitter2() );
			config.branchLength = config.branchLength * ( config.lengthShrink * jitter2() );
			vec3 xBasis = config.basis * vec3( 1.0f, 0.0f, 0.0f );
			vec3 zBasis = config.basis * vec3( 0.0f, 0.0f, 1.0f );
			config.basis = mat3( glm::rotate( config.branchTilt + jitter(), xBasis ) ) * config.basis;
			const float rotateIncrement = 6.28f / float( config.numBranches );
			for ( int i = 0; i < config.numBranches; i++ ) {
				TreeRecurse( config );
				config.basis = mat3( glm::rotate( rotateIncrement + jitter(), zBasis ) ) * config.basis;
			}
		}
	}

	void regenTree () {
		ChorizoConfig.geometryManager.ClearList();
		recursiveTreeConfig config;


		// for ( float x = -0.5f; x < 1.0f; x += 0.45f ) {
			palette::PickRandomPalette( true );
			config.basePoint = vec3( 0.0, 0.0f, -0.9f );
			TreeRecurse( config );
		// }



		// rngi numBranches = rngi( 2, 5 );
		// rng branchTilt = rng( 0.05f, 0.5f );
		// rng branchLength = rng( 0.2f, 0.7f );
		// rng branchRadius = rng( 0.01f, 0.1f );
		// rng lengthShrink = rng( 0.65f, 1.0f );
		// rng radiusShrink = rng( 0.65f, 1.0f );
		// rng paletteJitter = rng( 0.0f, 0.05f );
		// rngi maxLevels = rngi( 5, 12 );

		// config.numBranches = numBranches();
		// config.branchTilt = branchTilt();
		// config.branchLength = branchLength();
		// config.branchRadius = branchRadius();
		// config.lengthShrink = lengthShrink();
		// config.radiusShrink = radiusShrink();
		// config.paletteJitter = paletteJitter();
		// config.maxLevels = maxLevels();

	}

	void PrepSSBOs () {

		// // add some number of primitives to the buffer
		// const float xSpan = 1.6f;
		// const float xSpacing = 0.2f;
		// const float ySpan = 0.8f;
		// const float ySpacing = 0.2f;

		// for ( float x = -xSpan; x <= xSpan; x += xSpacing ) {
		// 	ChorizoConfig.geometryManager.AddCapsule( vec3( x, -ySpan, 0.0f ), vec3( x, ySpan, 0.0f ), 0.03f );
		// }
		// for ( float y = -ySpan; y <= ySpan; y += ySpacing ) {
		// 	ChorizoConfig.geometryManager.AddCapsule( vec3( -xSpan, y, 0.0f ), vec3( xSpan, y, 0.0f ), 0.03f );
		// }

		// for ( float x = -xSpan; x <= xSpan; x += xSpacing ) {
		// 	for ( float y = -ySpan; y <= ySpan; y += ySpacing ) {
		// 		ChorizoConfig.geometryManager.AddCapsule( vec3( x, y, 0.0f ), vec3( x + xSpacing, y, 0.0f ), 0.03f );
		// 		ChorizoConfig.geometryManager.AddCapsule( vec3( x, y, 0.0f ), vec3( x, y + ySpacing, 0.0f ), 0.03f );
		// 	}
		// }

		// rng pickKER = rng( -2.0f, 2.0f );
		// for ( int i = 0; i < 10000; i++ ) {
			// ChorizoConfig.geometryManager.AddCapsule( vec3( pickKER() * 0.3f, pickKER() * 0.3f, pickKER() ), vec3( pickKER() * 0.3f, pickKER() * 0.3f, pickKER() ), 0.003f );
		// }







		// tree-n-a
		// for ( float y = -2.0f; y < 2.0f; y += 0.125f ) {
		// 	config.basis = mat3( glm::rotate( 0.5f, vec3( 0.0f, 1.0f, 0.0f ) ) ) * config.basis;
		// 	config.basePoint.y = y;
		// 	TreeRecurse( config );
		// }



		// // rng pos = rng( -0.5f, 0.5f );
		// // rng scale = rng( 0.1f, 0.3f );
		// rng theta = rng( 0.0f, 6.28f );
		// rng phi = rng( -3.1415f, 3.1415f );
		// for ( float x = -3.0f; x < 3.0f; x += 0.3f ) {
		// 	for ( float y = -1.0f; y < 1.0f; y += 0.05 ) {
		// 		for ( float z = -1.0f; z < 1.0f; z += 0.05 ) {
		// 			float scaleFactor = pow( ( 3.0f - abs( x ) ) / 3.0f, 2.0f );
		// 			ChorizoConfig.geometryManager.AddRoundedBox( vec3( x, y, z ), vec3( 0.2f, 0.01f, 0.01f ), vec2( scaleFactor * theta(), scaleFactor * phi() ), 0.01f );
		// 		}
		// 	}
		// }


		regenTree();

		// rng pos = rng( -1.5f, 1.5f );
		// rng pos2 = rng( -0.2f, 0.2f );
		// rng scale = rng( 0.1f, 0.3f );
		// rng theta = rng( 0.0f, 6.28f );
		// rng phi = rng( -3.1415f, 3.1415f );
		// rng c = rng( 0.0f, 1.0f );

		// const int perBlock = 2000;

		// for ( int j = -3; j < 3; j++ ) {
		// 	palette::PickRandomPalette( true );
		// 	for ( int i = 0; i < perBlock; i++ ) {
		// 		vec3 color = palette::paletteRef( c() );
		// 		ChorizoConfig.geometryManager.AddRoundedBox( vec3( pos(), pos2(), pos2() ) + vec3( 0.0f, 0.0f, 1.0f * j ), vec3( 0.01f, scale(), 0.01f ), vec2( theta(), phi() ), 0.005f, color );
		// 	}
		// }

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
		const float time = SDL_GetTicks() / 10000.0f;

		// setting up the orbiting camera - this is getting very old, needs work
		ChorizoConfig.eyePosition = vec3(
			3.0f * sin( ChorizoConfig.ringPosition ),
			3.0f * cos( ChorizoConfig.ringPosition ),
			0.5f );

		const vec3 center = vec3( 0.0f, 0.0f, 1.5f );
		const vec3 up = vec3( 0.0f, 0.0f, -1.0f );
		vec3 perfectVec = ChorizoConfig.eyePosition - center;
		vec3 left = glm::cross( up, perfectVec );

		rng jitterAmount = rng( -ChorizoConfig.apertureAdjust, ChorizoConfig.apertureAdjust );
		ChorizoConfig.eyePosition += jitterAmount() * up + jitterAmount() * left;

		const float aspectRatio = float( config.width ) / float( config.height );

		ChorizoConfig.projTransform = glm::perspective( ChorizoConfig.FoV, aspectRatio, ChorizoConfig.nearZ, ChorizoConfig.farZ );
		ChorizoConfig.projTransformInverse = glm::inverse( ChorizoConfig.projTransform );

		ChorizoConfig.viewTransform = glm::lookAt( ChorizoConfig.eyePosition, vec3( 0.0f, 0.0f, 1.0f ) - perfectVec * ChorizoConfig.railAdjust, vec3( 0.0f, 0.0f, -1.0f ) );
		ChorizoConfig.viewTransformInverse = glm::inverse( ChorizoConfig.viewTransform );

		ChorizoConfig.combinedTransform = ChorizoConfig.projTransform * ChorizoConfig.viewTransform;
		ChorizoConfig.combinedTransformInverse = glm::inverse( ChorizoConfig.combinedTransform );

	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// // current state of the whole keyboard
		const uint8_t * state = SDL_GetKeyboardState( NULL );

		// // current state of the modifier keys
		// const SDL_Keymod k	= SDL_GetModState();
		// const bool shift		= ( k & KMOD_SHIFT );
		// const bool alt		= ( k & KMOD_ALT );
		// const bool control	= ( k & KMOD_CTRL );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

		if ( state[ SDL_SCANCODE_R ] ) {
			regenTree();
			ChorizoConfig.numPrimitives = ChorizoConfig.geometryManager.count;
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, ChorizoConfig.shapeParametersBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( vec4 ) * ChorizoConfig.numPrimitives * 4, ( GLvoid * ) ChorizoConfig.geometryManager.parametersList.data(), GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, ChorizoConfig.shapeParametersBuffer );
		}

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

		TonemapControlsWindow();

		ImGui::Begin( "Chorizo", NULL, ImGuiWindowFlags_NoScrollWithMouse );

		ImGui::SeparatorText( "= Controls =" );
		ImGui::SliderFloat( "FoV", &ChorizoConfig.FoV, 30.0f, 100.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
		ImGui::SliderFloat( "Blend Amount", &ChorizoConfig.blendAmount, 0.001f, 0.5f, "%.7f", ImGuiSliderFlags_Logarithmic );
		ImGui::SliderFloat( "Focus Adjust", &ChorizoConfig.railAdjust, -1.0f, 1.0f );
		ImGui::SliderFloat( "Aperture Adjust", &ChorizoConfig.apertureAdjust, 0.0f, 1.0f, "%.5f", ImGuiSliderFlags_Logarithmic );

		ImGui::Text( "" );

		ImGui::SeparatorText( "= Generator =" );

		ImGui::End();



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
			textureManager.BindImageForShader( "Accumulator", "accumulatorTexture", shader, 1 );
			textureManager.BindTexForShader( "Framebuffer Depth " + index, "depthTex", shader, 2 );
			textureManager.BindTexForShader( "Framebuffer Depth " + indexBack, "depthTexBack", shader, 3 );
			textureManager.BindTexForShader( "Framebuffer Normal " + index, "normals", shader, 4 );
			textureManager.BindTexForShader( "Framebuffer Normal " + indexBack, "normalsBack", shader, 5 );
			textureManager.BindTexForShader( "Framebuffer Primitive ID " + index, "primitiveID", shader, 6 );
			textureManager.BindTexForShader( "Framebuffer Primitive ID " + indexBack, "primitiveIDBack", shader, 7 );

			glUniformMatrix4fv( glGetUniformLocation( shader, "combinedTransform" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.combinedTransform ) );
			glUniformMatrix4fv( glGetUniformLocation( shader, "combinedTransformInverse" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.combinedTransformInverse ) );

			glUniform1i( glGetUniformLocation( shader, "inSeed" ), ChorizoConfig.wangSeeder() );
			glUniform1f( glGetUniformLocation( shader, "nearZ" ), ChorizoConfig.nearZ );
			glUniform1f( glGetUniformLocation( shader, "farZ" ), ChorizoConfig.farZ );
			glUniform1f( glGetUniformLocation( shader, "blendAmount" ), ChorizoConfig.blendAmount );

			// glUniformMatrix4fv( glGetUniformLocation( shader, "viewTransform" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.viewTransform ) );
			// glUniformMatrix4fv( glGetUniformLocation( shader, "viewTransformInverse" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.viewTransformInverse ) );

			// glUniformMatrix4fv( glGetUniformLocation( shader, "projTransform" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.projTransform ) );
			// glUniformMatrix4fv( glGetUniformLocation( shader, "projTransformInverse" ), 1, GL_FALSE, glm::value_ptr( ChorizoConfig.projTransformInverse ) );

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

#include "../../../engine/engine.h"
#include "daedalusConfig.h"

class Daedalus final : public engineBase {
public:
	Daedalus () { Init(); OnInit(); PostInit(); }
	~Daedalus () { Quit(); }

	daedalusConfig_t daedalusConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			// to put the data into the accumulator
			CompileShaders();

			// create the new accumulator(s)
			textureOptions_t opts;
			opts.dataType		= GL_RGBA32F;
			opts.width			= daedalusConfig.targetWidth;
			opts.height			= daedalusConfig.targetHeight;
			opts.minFilter		= GL_LINEAR;
			opts.magFilter		= GL_LINEAR;
			opts.textureType	= GL_TEXTURE_2D;
			opts.wrap			= GL_CLAMP_TO_BORDER;
			textureManager.Add( "Color Accumulator", opts );
			textureManager.Add( "Color Accumulator Scratch", opts );
			textureManager.Add( "Tonemapped", opts );
			textureManager.Add( "Depth/Normals Accumulator", opts );

			// and the texture for the sky
			opts.dataType		= GL_RGBA32F;
			opts.wrap			= GL_CLAMP_TO_EDGE;
			opts.width			= daedalusConfig.render.scene.skyDims.x;
			opts.height			= daedalusConfig.render.scene.skyDims.y;
			textureManager.Add( "Sky Cache", opts );

			// more to do on this, but... yeah, it'll work
			glGenBuffers( 1, &daedalusConfig.render.scene.explicitPrimitiveSSBO );
			PrepSphereBufferRandom();
			RelaxSphereList();
			SendExplicitPrimitiveSSBO();

			// prepare the glyph and DDA VAT buffers
			PrepGlyphBuffer();
			DDAVATTex();
			// HeightmapTex();

			{ // color grading tools
				// exposure/brightness/color histogram setup
				glGenBuffers( 1, &daedalusConfig.render.grading.colorHistograms );

				opts = textureOptions_t();

				// histogram presentation buffer
				opts.textureType	= GL_TEXTURE_2D;
				opts.dataType		= GL_RGBA8;
				opts.magFilter		= GL_NEAREST;
				opts.minFilter		= GL_NEAREST;
				opts.wrap			= GL_CLAMP_TO_BORDER;
				opts.width			= 256;
				opts.height			= 64;	// 16 per, not needing a lot of resolution
				textureManager.Add( "Histogram Composite", opts );

				// waveform setup - images all need to be re-created on target resize
				// min/max + global max buffers
				glGenBuffers( 1, &daedalusConfig.render.grading.waveformMaxs );
				glGenBuffers( 1, &daedalusConfig.render.grading.waveformMins );
				glGenBuffers( 1, &daedalusConfig.render.grading.waveformGlobalMax );

				// data accumulation buffer
				opts.dataType		= GL_R32UI;
				opts.width			= daedalusConfig.targetWidth;
				opts.height			= 256;
				textureManager.Add( "Waveform Red", opts );
				textureManager.Add( "Waveform Green", opts );
				textureManager.Add( "Waveform Blue", opts );
				textureManager.Add( "Waveform Luma", opts );

				// present buffer
				// parade graph, separate R, G, B, Luma
				opts.dataType		= GL_RGBA8;
				opts.minFilter		= GL_LINEAR;
				opts.magFilter		= GL_LINEAR;
				opts.height			= 1024;
				textureManager.Add( "Parade Composite", opts );

				// waveform, with RGB stacked
				opts.height			= 256;
				textureManager.Add( "Waveform Composite", opts );

				// data for the vectorscope (hue/chroma plot)
				glGenBuffers( 1, &daedalusConfig.render.grading.vectorscopeMax );

				// data accumulation buffer
				opts.width			= 512;
				opts.height			= 512;
				opts.dataType		= GL_R32UI;
				opts.minFilter		= GL_NEAREST;
				opts.magFilter		= GL_NEAREST;
				textureManager.Add( "Vectorscope Data", opts );

				// vectorscope presentation buffer
				opts.dataType		= GL_RGBA8;
				opts.minFilter		= GL_LINEAR;
				opts.magFilter		= GL_LINEAR;
				textureManager.Add( "Vectorscope Composite", opts );
			}
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// // current state of the whole keyboard
		const uint8_t * state = SDL_GetKeyboardState( NULL );

		// current state of the modifier keys
		const SDL_Keymod k	= SDL_GetModState();
		const bool shift		= ( k & KMOD_SHIFT );
		// const bool alt		= ( k & KMOD_ALT );
		const bool control		= ( k & KMOD_CTRL );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

		if ( state[ SDL_SCANCODE_R ] && shift ) {
			ResetAccumulators();
		}

		if ( state[ SDL_SCANCODE_Y ] && shift ) {
			cout << "Compiling shaders...";
			CompileShaders();
			cout << " done.";
		}

		// quaternion based rotation via retained state in the basis vectors
		const float scalar = shift ? 0.1f : ( control ? 0.0005f : 0.02f );
		if ( state[ SDL_SCANCODE_W ] ) {
			glm::quat rot = glm::angleAxis( scalar, daedalusConfig.render.basisX ); // basisX is the axis, therefore remains untransformed
			daedalusConfig.render.basisY = ( rot * vec4( daedalusConfig.render.basisY, 0.0f ) ).xyz();
			daedalusConfig.render.basisZ = ( rot * vec4( daedalusConfig.render.basisZ, 0.0f ) ).xyz();
		}
		if ( state[ SDL_SCANCODE_S ] ) {
			glm::quat rot = glm::angleAxis( -scalar, daedalusConfig.render.basisX );
			daedalusConfig.render.basisY = ( rot * vec4( daedalusConfig.render.basisY, 0.0f ) ).xyz();
			daedalusConfig.render.basisZ = ( rot * vec4( daedalusConfig.render.basisZ, 0.0f ) ).xyz();
		}
		if ( state[ SDL_SCANCODE_A ] ) {
			glm::quat rot = glm::angleAxis( -scalar, daedalusConfig.render.basisY ); // same as above, but basisY is the axis
			daedalusConfig.render.basisX = ( rot * vec4( daedalusConfig.render.basisX, 0.0f ) ).xyz();
			daedalusConfig.render.basisZ = ( rot * vec4( daedalusConfig.render.basisZ, 0.0f ) ).xyz();
		}
		if ( state[ SDL_SCANCODE_D ] ) {
			glm::quat rot = glm::angleAxis( scalar, daedalusConfig.render.basisY );
			daedalusConfig.render.basisX = ( rot * vec4( daedalusConfig.render.basisX, 0.0f ) ).xyz();
			daedalusConfig.render.basisZ = ( rot * vec4( daedalusConfig.render.basisZ, 0.0f ) ).xyz();
		}
		if ( state[ SDL_SCANCODE_Q ] ) {
			glm::quat rot = glm::angleAxis( scalar, daedalusConfig.render.basisZ ); // and again for basisZ
			daedalusConfig.render.basisX = ( rot * vec4( daedalusConfig.render.basisX, 0.0f ) ).xyz();
			daedalusConfig.render.basisY = ( rot * vec4( daedalusConfig.render.basisY, 0.0f ) ).xyz();
		}
		if ( state[ SDL_SCANCODE_E ] ) {
			glm::quat rot = glm::angleAxis( -scalar, daedalusConfig.render.basisZ );
			daedalusConfig.render.basisX = ( rot * vec4( daedalusConfig.render.basisX, 0.0f ) ).xyz();
			daedalusConfig.render.basisY = ( rot * vec4( daedalusConfig.render.basisY, 0.0f ) ).xyz();
		}

		// zoom in and out with plus/minus
		if ( state[ SDL_SCANCODE_MINUS ] ) {
			daedalusConfig.render.FoV += scalar;
		}
		if ( state[ SDL_SCANCODE_EQUALS ] ) {
			daedalusConfig.render.FoV -= scalar;
		}

		// f to reset basis, shift + f to reset basis and home to origin
		if ( state[ SDL_SCANCODE_F ] ) {
			if ( shift ) daedalusConfig.render.viewerPosition = vec3( 0.0f, 0.0f, 0.0f );
			daedalusConfig.render.basisX = vec3( 1.0f, 0.0f, 0.0f );
			daedalusConfig.render.basisY = vec3( 0.0f, 1.0f, 0.0f );
			daedalusConfig.render.basisZ = vec3( 0.0f, 0.0f, 1.0f );
		}
		if ( state[ SDL_SCANCODE_UP ] )			daedalusConfig.render.viewerPosition += scalar * daedalusConfig.render.basisZ;
		if ( state[ SDL_SCANCODE_DOWN ] )		daedalusConfig.render.viewerPosition -= scalar * daedalusConfig.render.basisZ;
		if ( state[ SDL_SCANCODE_RIGHT ] )		daedalusConfig.render.viewerPosition += scalar * daedalusConfig.render.basisX;
		if ( state[ SDL_SCANCODE_LEFT ] )		daedalusConfig.render.viewerPosition -= scalar * daedalusConfig.render.basisX;
		if ( state[ SDL_SCANCODE_PAGEDOWN ] )	daedalusConfig.render.viewerPosition += scalar * daedalusConfig.render.basisY;
		if ( state[ SDL_SCANCODE_PAGEUP ] )		daedalusConfig.render.viewerPosition -= scalar * daedalusConfig.render.basisY;

		// watch for space being held, when things become unresponsive ( not a toggle, only kill )
		if ( state[ SDL_SCANCODE_SPACE ] ) daedalusConfig.render.render = false;

		ivec2 mouse;
		uint32_t mouseState = SDL_GetMouseState( &mouse.x, &mouse.y );
		if ( mouseState != 0 && !ImGui::GetIO().WantCaptureMouse ) {
			if ( shift == true ) { // autofocus
				daedalusConfig.view.clickPosition = vec2( mouse.x, config.height - mouse.y );
				vec2 pixelLocation = daedalusConfig.view.clickPosition * daedalusConfig.view.outputZoom + daedalusConfig.view.outputOffset;
				if ( pixelLocation.x >= 0 && pixelLocation.y >= 0 && pixelLocation.x < daedalusConfig.targetWidth && pixelLocation.y < daedalusConfig.targetHeight ) {
					float value[ 4 ]; // get the value from the depth buffer - determine where to set the focal plane for the DoF
					const GLuint texture = textureManager.Get( "Depth/Normals Accumulator" );
					glGetTextureSubImage( texture, 0, pixelLocation.x,  daedalusConfig.targetHeight - pixelLocation.y, 0, 1, 1, 1, GL_RGBA, GL_FLOAT, 16, &value );
					daedalusConfig.render.thinLensFocusDistance = value[ 3 ];
				}
			} else { // panning control
				ImVec2 currentMouseDrag = ImGui::GetMouseDragDelta( 0 );
				ImGui::ResetMouseDragDelta();
				const float aspectRatio = ( float ) daedalusConfig.targetHeight / ( float ) daedalusConfig.targetWidth;
				daedalusConfig.view.outputOffset.x -= currentMouseDrag.x * aspectRatio * daedalusConfig.view.outputZoom;
				daedalusConfig.view.outputOffset.y += currentMouseDrag.y * daedalusConfig.view.outputZoom;
			}
		}

		SDL_Event event;
		SDL_PumpEvents();
		while ( SDL_PollEvent( &event ) ) {
			ImGui_ImplSDL2_ProcessEvent( &event ); // imgui event handling
			pQuit = config.oneShot || // swap out the multiple if statements for a big chained boolean setting the value of pQuit
				( event.type == SDL_QUIT ) ||
				( event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID( window.window ) ) ||
				( event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE && SDL_GetModState() & KMOD_SHIFT );
			if ( ( event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE ) || ( event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_X1 ) ) {
				quitConfirm = !quitConfirm; // this has to stay because it doesn't seem like ImGui::IsKeyReleased is stable enough to use
			}

			if ( event.type == SDL_MOUSEWHEEL && !ImGui::GetIO().WantCaptureMouse ) {
				// float wheel_x = -event.wheel.x;
				const float wheel_y = event.wheel.y;

				const float previousZoom = daedalusConfig.view.outputZoom;
				const vec2 previousOffset = daedalusConfig.view.outputOffset;

				// would also be nice if this could have a little bit of smoothness added to it, inertia, whatever
				daedalusConfig.view.outputZoom -= wheel_y * ( ( SDL_GetModState() & KMOD_SHIFT ) ? 0.07f : 0.01f );
				daedalusConfig.view.outputZoom = std::clamp( daedalusConfig.view.outputZoom, 0.005f, 5.0f );

				// closer, but still not correct
				const vec2 previousPixelLocation = ( previousOffset + vec2( mouse ) ) * previousZoom;
				daedalusConfig.view.outputOffset = ( previousPixelLocation - vec2( mouse ) * daedalusConfig.view.outputZoom ) / daedalusConfig.view.outputZoom;
			}
		}

	}

	void UpdatePerfMonitor ( float loopTime, uint32_t tileCount ) {
		ZoneScoped;
		daedalusConfig.timeHistory.push_back( loopTime );
		daedalusConfig.timeHistory.pop_front();
	}

	void ResetAccumulators();
	void ResizeAccumulators( uint32_t x, uint32_t y );
	void Screenshot( string label, bool srgbConvert = true, bool fullDepth = false );
	void ApplyFilter( int mode, int count );

	void ShowDaedalusConfigWindow();
	void ImguiPass() {
		ZoneScoped;
		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
		if ( daedalusConfig.showConfigWindow == true ) { ShowDaedalusConfigWindow(); }
		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}
		QuitConf( &quitConfirm ); // show quit confirm window, if triggered
	}

	void SendSkyCacheUniforms();
	void SendBasePathtraceUniforms();
	void SendInnerLoopPathtraceUniforms();
	void SendPrepareUniforms();
	void SendPresentUniforms();
	void SendWaveformPrepareUniforms();
	void SendWaveformCompositeUniforms( int mode );
	void SendVectorscopePrepareUniforms();
	void SendVectorscopeCompositeUniforms();

	void PrepSphereBufferRandom();
	void PrepSphereBufferPacking();
	void RelaxSphereList();
	void SendExplicitPrimitiveSSBO();
	void PrepGlyphBuffer();
	void DDAVATTex();
	void HeightmapTex();
	void ClearColorGradingHistogramBuffer();
	void ClearColorGradingWaveformBuffer();
	void ClearColorGradingVectorscopeBuffer();
	void LoadSkyBoxEXRFromString( string label );

	GLuint64 SubmitTimerAndWait( GLuint timer ) {
		ZoneScoped;
		glQueryCounter( timer, GL_TIMESTAMP );
		GLint available = 0;
		while ( !available ) { glGetQueryObjectiv( timer, GL_QUERY_RESULT_AVAILABLE, &available ); }
		GLuint64 t;
		glGetQueryObjectui64v( timer, GL_QUERY_RESULT, &t );
		return t;
	}

	void CompileShaders() {
		shaders[ "Sky Cache" ]			= computeShader( "./src/projects/PathTracing/Daedalus/shaders/skyCache.cs.glsl" ).shaderHandle;
		shaders[ "Pathtrace" ]			= computeShader( "./src/projects/PathTracing/Daedalus/shaders/pathtrace.cs.glsl" ).shaderHandle;
		shaders[ "Prepare" ]			= computeShader( "./src/projects/PathTracing/Daedalus/shaders/prepare.cs.glsl" ).shaderHandle;
		shaders[ "Present" ]			= computeShader( "./src/projects/PathTracing/Daedalus/shaders/present.cs.glsl" ).shaderHandle;
		shaders[ "Filter" ]				= computeShader( "./src/projects/PathTracing/Daedalus/shaders/filter.cs.glsl" ).shaderHandle;
		shaders[ "Copy Back" ]			= computeShader( "./src/projects/PathTracing/Daedalus/shaders/copyBack.cs.glsl" ).shaderHandle;
		shaders[ "Histogram" ]			= computeShader( "./src/projects/PathTracing/Daedalus/shaders/histogram.cs.glsl" ).shaderHandle;
		shaders[ "Waveform Prepare" ]	= computeShader( "./src/projects/PathTracing/Daedalus/shaders/waveformPrepare.cs.glsl" ).shaderHandle;
		shaders[ "Waveform Composite" ]	= computeShader( "./src/projects/PathTracing/Daedalus/shaders/waveformComposite.cs.glsl" ).shaderHandle;
		shaders[ "Parade Composite" ]	= computeShader( "./src/projects/PathTracing/Daedalus/shaders/paradeComposite.cs.glsl" ).shaderHandle;
		shaders[ "Vectorscope Prepare" ] = computeShader( "./src/projects/PathTracing/Daedalus/shaders/vectorscopePrepare.cs.glsl" ).shaderHandle;
		shaders[ "Vectorscope Composite" ] = computeShader( "./src/projects/PathTracing/Daedalus/shaders/vectorscopeComposite.cs.glsl" ).shaderHandle;
		daedalusConfig.render.scene.skyNeedsUpdate = true;
	}

	void ComputePasses () {
		ZoneScoped;

		if ( daedalusConfig.filterEveryFrame ) {
			ApplyFilter( daedalusConfig.filterSelector, 1 );
		}

		{
			scopedTimer Start( "Sky Cache" );
			if ( daedalusConfig.render.scene.skyNeedsUpdate == true && daedalusConfig.render.scene.skyMode != 6 ) {
				daedalusConfig.render.scene.skyNeedsUpdate = false;
				// dispatch for every pixel in the sky cache texture
				SendSkyCacheUniforms();
				glDispatchCompute( daedalusConfig.render.scene.skyDims.x / 16, daedalusConfig.render.scene.skyDims.y / 16, 1 );
				glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
			}
		}

		{ // do some tiles, update the buffer
			scopedTimer Start( "Tiled Update" );

			const GLuint shader = shaders[ "Pathtrace" ];
			glUseProgram( shader );

			// create OpenGL timery query objects - more reliable than std::chrono, at least in theory
			GLuint t[ 2 ];
			GLuint64 tStart;
			glGenQueries( 2, t );

			// submit the first timer query, to determine tStart, outside the loop
			tStart = SubmitTimerAndWait( t[ 0 ] );

			// for monitoring number of completed tiles
			uint32_t tilesThisFrame = 0;
			SendBasePathtraceUniforms();
			const uint32_t tileSize = daedalusConfig.tiles.tileSize;
			while ( daedalusConfig.render.render && !quitConfirm ) {
				for ( uint32_t tile = 0; tile < daedalusConfig.tiles.tilesBetweenQueries; tile++ ) {
					SendInnerLoopPathtraceUniforms();
					glDispatchCompute( tileSize / 16, tileSize / 16, 1 );
					tilesThisFrame++;
				}
				float loopTime = ( SubmitTimerAndWait( t[ 1 ] ) - tStart ) / 1e6f; // convert ns -> ms
				if ( loopTime > daedalusConfig.tiles.tileTimeLimitMS ) {
					UpdatePerfMonitor( loopTime, tilesThisFrame );
					break;
				}
			}
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{	// this is the tonemapping stage, on the result ( accumulator(s) -> tonemapped )
			scopedTimer Start( "Prepare" );
			const GLuint shader = shaders[ "Prepare" ];
			glUseProgram( shader );
			SendPrepareUniforms();

			if ( daedalusConfig.render.grading.updateHistogram == true ) {
				ClearColorGradingHistogramBuffer();
			}

			glDispatchCompute( ( daedalusConfig.targetWidth + 15 ) / 16, ( daedalusConfig.targetHeight + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		if ( daedalusConfig.render.grading.updateHistogram == true ) {	// prepping the image of the color/luminance histogram
			scopedTimer Start( "Histogram Prep" );
			const GLuint shader = shaders[ "Histogram" ];
			glUseProgram( shader );
			textureManager.BindImageForShader( "Histogram Composite", "histogram", shader, 0 );
			glDispatchCompute( 256 / 16, 64 / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		if ( daedalusConfig.render.grading.updateWaveform == true || daedalusConfig.render.grading.updateParade == true ) {	// prepping the waveform display
			scopedTimer Start( "Waveform Prep/Composite" );

			// initialize the min/max buffers
			ClearColorGradingWaveformBuffer();
			glMemoryBarrier( GL_BUFFER_UPDATE_BARRIER_BIT );

			// prepare the data
			const GLuint prepareShader = shaders[ "Waveform Prepare" ];
			glUseProgram( prepareShader );
			SendWaveformPrepareUniforms();
			glDispatchCompute( ( daedalusConfig.targetWidth + 15 ) / 16, ( daedalusConfig.targetHeight + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

			if ( daedalusConfig.render.grading.updateWaveform ) {
				// create the waveform image
				const GLuint compositeShader = shaders[ "Waveform Composite" ];
				glUseProgram( compositeShader );
				SendWaveformCompositeUniforms( 1 );
				glDispatchCompute( ( daedalusConfig.targetWidth + 15 ) / 16, 16, 1 ); // height of 256
				glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
			}

			if ( daedalusConfig.render.grading.updateParade ) {
				// create the waveform image
				const GLuint compositeShader = shaders[ "Parade Composite" ];
				glUseProgram( compositeShader );
				SendWaveformCompositeUniforms( 0 );
				glDispatchCompute( ( daedalusConfig.targetWidth + 15 ) / 16, 64, 1 ); // height of 1024
				glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
			}
		}

		if ( daedalusConfig.render.grading.updateVectorscope == true ) {
			scopedTimer Start( "Vectorscope Prep/Composite" );

			// initialize the max count value
			ClearColorGradingVectorscopeBuffer();
			glMemoryBarrier( GL_BUFFER_UPDATE_BARRIER_BIT );

			// prepare the data
			const GLuint prepareShader = shaders[ "Vectorscope Prepare" ];
			glUseProgram( prepareShader );
			SendVectorscopePrepareUniforms();
			glDispatchCompute( ( daedalusConfig.targetWidth + 15 ) / 16, ( daedalusConfig.targetHeight + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

			// create the vectorscope image
			const GLuint compositeShader = shaders[ "Vectorscope Composite" ];
			glUseProgram( compositeShader );
			SendVectorscopeCompositeUniforms();
			glDispatchCompute( 512 / 16, 512 / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{	// this is the matting, with guides
			scopedTimer Start( "Drawing" );
			const GLuint shader = shaders[ "Present" ];
			glUseProgram( shader );
			SendPresentUniforms();
			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}

		{ // text rendering - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );

			// toggle-able controls list, sounds like a nice to have
			// textRenderer.DrawBlackBackedColorString( 2, string( " Daedalus  (?) show controls" ), vec3( 1.0f, 0.5f, 0.3f ) );

			float seconds = daedalusConfig.tiles.SecondsSinceLastReset();
			float wholeSeconds = std::floor( seconds );
			stringstream ss;
			// ss << fixedWidthNumberString( wholeSeconds, 5, ' ' ) << "." << fixedWidthNumberString( ( seconds - wholeSeconds ) * 1000, 3, '0' ) << "s";
			ss << fixedWidthNumberString( daedalusConfig.tiles.SampleCount(), 6, ' ' ) << " samples in ";
			ss << fixedWidthNumberString( int( wholeSeconds ) / 3600, 2, ' ' ) << "h " <<
				fixedWidthNumberString( ( int( wholeSeconds ) / 60 ) % 60, 2, ' ' ) << "m " <<
				fixedWidthNumberString( int( wholeSeconds ) % 60, 2, '0' ) << "s";

			textRenderer.DrawBlackBackedColorString( 1, ss.str(), vec3( 1.0f ) );
			ss.str( "" );
			float tDelta = ImGui::GetIO().DeltaTime * 1000.0f;
			ss << " frame total:    " << std::setw( 10 ) << std::setfill( ' ' ) << std::setprecision( 4 ) << std::fixed << tDelta << "ms";
			textRenderer.DrawBlackBackedColorString( 0, ss.str(), vec3( 1.0f ) );

			// kill if the shaders fail - this is a temporary measure, eventually I'd like to incorporate this into the shader wrapper
			// if ( tDelta > 1000.0f ) pQuit = true;
			
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
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

		HandleCustomEvents();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};
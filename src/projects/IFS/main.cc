#include "../../engine/engine.h"
#include "operations.h"

class ifs final : public engineBase { // sample derived from base engine class
public:
	ifs () { Init(); OnInit(); PostInit(); }
	~ifs () { Quit(); }

	GLuint maxBuffer;
	GLuint dataBuffer;

	// IFS view parameters
	float scale = 1.0f;
	vec2 offset = vec2( 0.0f );

	// output prep
	float brightness = 100.0f;
	float brightnessPower = 1.0f;
	bool screenshotRequested = false;

	// flag for field wipe (on zoom, drag, etc)
	bool RendererNeedsReset = false;

	// flag for SSBO update
	bool BufferNeedsReset = false;

	// operations list
	std::vector< operation_t > currentOperations;
	int initMode = 0;

	void LoadShaders() {
		shaders[ "Draw" ] = computeShader( "./src/projects/IFS/shaders/draw.cs.glsl" ).shaderHandle;
		shaders[ "Update" ] = computeShader( "./src/projects/IFS/shaders/update.cs.glsl" ).shaderHandle;
		shaders[ "Clear" ] = computeShader( "./src/projects/IFS/shaders/clear.cs.glsl" ).shaderHandle;

		glObjectLabel( GL_PROGRAM, shaders[ "Draw" ], -1, string( "Draw" ).c_str() );
		glObjectLabel( GL_PROGRAM, shaders[ "Update" ], -1, string( "Update" ).c_str() );
		glObjectLabel( GL_PROGRAM, shaders[ "Clear" ], -1, string( "Clear" ).c_str() );
	}

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			LoadShaders();

			textureOptions_t opts;
			opts.width = config.width;
			opts.height = config.height;
			opts.dataType = GL_R32UI;
			opts.textureType = GL_TEXTURE_2D;
			textureManager.Add( "IFS Accumulator R", opts );
			textureManager.Add( "IFS Accumulator G", opts );
			textureManager.Add( "IFS Accumulator B", opts );

			// buffer holding the normalize factor
			glGenBuffers( 1, &maxBuffer );
			constexpr uint32_t countValue = 0;
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, maxBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, 4, ( GLvoid * ) &countValue, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, maxBuffer );

			// setup labels for imgui
			PopulateLabels();

			// populate the current list of operations
			for ( int i = 0; i < 6; i++ ) {
				currentOperations.push_back( GetRandomOperation() );
			}

			glGenBuffers( 1, &dataBuffer );
			PassOperationData();
		}
	}

	void PassOperationData () {
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, dataBuffer );
		std::vector< vec4 > data;

		for ( uint i = 0; i < currentOperations.size(); i++ ) {
			// operation information ( operation, input swizzle, output swizzle, unused )
			data.push_back( vec4( currentOperations[ i ].index, currentOperations[ i ].inputSwizzle, currentOperations[ i ].outputSwizzle, 0.0f ) );
			
			// arguments ( x,y,z are used, selectively, by some operations, fourth is unused )
			data.push_back( currentOperations[ i ].args );
			
			// color (r,g,b,unused alpha channel)
			data.push_back( currentOperations[ i ].color );
		}

		size_t numBytes = sizeof( float ) * 4 * 3 * currentOperations.size();
		glBufferData( GL_SHADER_STORAGE_BUFFER, numBytes, ( GLvoid * ) &data[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, dataBuffer );
	}

	void HandleQuitEvents () {
		ZoneScoped;
		//==============================================================================
		// Need to keep this for pQuit handling ( force quit )
		// In particular - checking for window close and the SDL_QUIT event can't really be determined
		//  via the keyboard state, and then imgui needs it too, so can't completely kill the event
		//  polling loop - maybe eventually I'll find a more complete solution for this
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
				scale *= ( wheel_y > 0.0f ) ? wheel_y * ( ( SDL_GetModState() & KMOD_SHIFT ) ? 1.01f : 1.1f ) : abs( wheel_y ) * ( ( SDL_GetModState() & KMOD_SHIFT ) ? 0.99f : 0.9f );
				brightness *= ( wheel_y > 0.0f ) ? wheel_y * ( ( SDL_GetModState() & KMOD_SHIFT ) ? 1.01f : 1.1f ) : abs( wheel_y ) * ( ( SDL_GetModState() & KMOD_SHIFT ) ? 0.99f : 0.9f );
				RendererNeedsReset = true;
			}
		}
	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// current state of the whole keyboard
		const uint8_t * state = SDL_GetKeyboardState( NULL );

		// // current state of the modifier keys
		const SDL_Keymod k	= SDL_GetModState();
		const bool shift		= ( k & KMOD_SHIFT );
		// const bool alt		= ( k & KMOD_ALT );
		// const bool control	= ( k & KMOD_CTRL );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

		if ( state[ SDL_SCANCODE_R ] && shift ) {
			// reset
			RendererNeedsReset = true;
			scale = 1.0f;
			offset = vec2( 0.0f );
			trident.InitBasis();
		}

		if ( state[ SDL_SCANCODE_Y ] && shift ) {
			// reload shaders
			LoadShaders();
		}

		ivec2 mouse;
		uint32_t mouseState = SDL_GetMouseState( &mouse.x, &mouse.y );
		if ( mouseState != 0 && !ImGui::GetIO().WantCaptureMouse ) {
			ImVec2 currentMouseDrag = ImGui::GetMouseDragDelta( 0 );
			ImGui::ResetMouseDragDelta();
			const float aspectRatio = ( float ) config.height / ( float ) config.width;
			offset.x += currentMouseDrag.x * aspectRatio * 0.002f / scale;
			offset.y += currentMouseDrag.y * 0.002f / scale;
			RendererNeedsReset = true;
		}
	}

	void ImguiPass () {
		ZoneScoped;
	
		ImGui::Begin( "Controls" );
		if ( ImGui::Button( " Capture " ) ) {
			screenshotRequested = true;
		}
		ImGui::SameLine();
		if ( ImGui::Button( " Capture EXR " ) ) {
			// todo - I want EXRs for HDRI usage
		}
		ImGui::SliderFloat( "Scale", &scale, 0.0f, 100.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
		RendererNeedsReset = RendererNeedsReset || ImGui::IsItemEdited();
		ImGui::SliderFloat( "X Offset", &offset.x, -100.0f, 100.0f );
		RendererNeedsReset = RendererNeedsReset || ImGui::IsItemEdited();
		ImGui::SliderFloat( "Y Offset", &offset.y, -100.0f, 100.0f );
		RendererNeedsReset = RendererNeedsReset || ImGui::IsItemEdited();
		ImGui::SliderFloat( "Brightness", &brightness, 0.0f, 100000.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
		ImGui::SliderFloat( "Brightness Power", &brightnessPower, 0.0f, 10.0f, "%.5f", ImGuiSliderFlags_Logarithmic );
		ImGui::End();

		// =================================
		ImGui::Begin( "Operations" );

		// showing current palette? not sure how I want to handle that
			// I would like something like the old palette selector, and when the parameters change, generate new values
			// for all operations

		if ( ImGui::Button( "New Palette" ) ) {
			rng colorInit = rng( 0.0f, 1.0f );
			palette::PickRandomPalette( true );
			for ( uint i = 0; i < currentOperations.size(); i++ ) {
				currentOperations[ i ].color = vec4( palette::paletteRef( colorInit() ), 1.0f );
			}
			BufferNeedsReset = true;
			RendererNeedsReset = true;
		}

		// =================================
		ImGui::Text( " " );
		ImGui::Text( "Seeding" );
		ImGui::Indent();
		ImGui::Combo( "Initial Point Distribution", &initMode, initializationLabels, INIT_MODES_COUNT );
		ImGui::Unindent();
		// =================================
		ImGui::SameLine();
		if ( ImGui::Button( "Randomize All" ) ) {
			for ( uint i = 0; i < currentOperations.size(); i++ ) {
				currentOperations[ i ] = GetRandomOperation();
			}
			BufferNeedsReset = true;
			RendererNeedsReset = true;
		}

		// showing palette contents, tbd

		for ( uint i = 0; i < currentOperations.size(); i++ ) { // list out the current operations
			string label = string( "Operation " ) + std::to_string( i );
			ImGui::Text( label.c_str() );
			ImGui::SameLine();
			if ( ImGui::Button( ( string( " Remove ##" + std::to_string( i ) ) ).c_str() ) ) {
				currentOperations.erase( currentOperations.begin() + i );
				BufferNeedsReset = true;
				RendererNeedsReset = true;
			}
			ImGui::SameLine();
			if ( ImGui::Button( ( string( " Randomize ##" + std::to_string( i ) ) ).c_str() ) ) {
				currentOperations.erase( currentOperations.begin() + i );
				currentOperations.insert( currentOperations.begin() + i, GetRandomOperation() );
				BufferNeedsReset = true;
				RendererNeedsReset = true;
			}

			ImGui::Separator();
			ImGui::Indent();

			// type of operation specified
			string typeLabel = string( "Type##" ) + std::to_string( i );
			ImGui::Combo( typeLabel.c_str(), ( int * ) &currentOperations[ i ].index, operationLabels, NUM_OPERATIONS );
			BufferNeedsReset |= ImGui::IsItemEdited();

			// input swizzle
			int dimensions = 0; int offset = 0; int count = 0;
			switch ( operationList[ currentOperations[ i ].index ].inputSize ) {
				case 1: dimensions = 1; offset =  1; count = 3; break;
				case 2: dimensions = 2; offset =  4; count = 6; break;
				case 3: dimensions = 3; offset = 10; count = 6; break;
				default: break;
			}
			string inputSwizzleLabel = string( "Input Swizzle (" ) + std::to_string( dimensions ) + string( "D)##" ) + std::to_string( i );
			currentOperations[ i ].inputSwizzle = swizzle( int( currentOperations[ i ].inputSwizzle ) - offset );
			ImGui::Combo( inputSwizzleLabel.c_str(), ( int * ) &currentOperations[ i ].inputSwizzle, swizzleLabels + offset, count );
			currentOperations[ i ].inputSwizzle = swizzle( int( currentOperations[ i ].inputSwizzle ) + offset );
			BufferNeedsReset |= ImGui::IsItemEdited();

			// output swizzle
			dimensions = 0; offset = 0; count = 0;
			switch ( operationList[ currentOperations[ i ].index ].outputSize ) {
				case 1: dimensions = 1; offset =  1; count = 3; break;
				case 2: dimensions = 2; offset =  4; count = 6; break;
				case 3: dimensions = 3; offset = 10; count = 6; break;
				default: break;
			}
			string outputSwizzleLabel = string( "Output Swizzle (" ) + std::to_string( dimensions ) + string( "D)##" ) + std::to_string( i );
			currentOperations[ i ].outputSwizzle = swizzle( int( currentOperations[ i ].outputSwizzle ) - offset );
			ImGui::Combo( outputSwizzleLabel.c_str(), ( int * ) &currentOperations[ i ].outputSwizzle, swizzleLabels + offset, count );
			currentOperations[ i ].outputSwizzle = swizzle( int( currentOperations[ i ].outputSwizzle ) + offset );
			BufferNeedsReset |= ImGui::IsItemEdited();

			int numArgs = operationList[ currentOperations[ i ].index ].numArgs;
			string args = string( "Arguments##" ) + std::to_string( i );
			switch ( numArgs ) {
				case 0: break; // no op
				case 1:
					ImGui::SliderFloat( args.c_str(), ( float * ) &currentOperations[ i ].args, -2.0f, 2.0f );
					BufferNeedsReset |= ImGui::IsItemEdited();
					break;
				case 2:
					ImGui::SliderFloat2( args.c_str(), ( float * ) &currentOperations[ i ].args, -2.0f, 2.0f );
					BufferNeedsReset |= ImGui::IsItemEdited();
					break;
				case 3:
					ImGui::SliderFloat3( args.c_str(), ( float * ) &currentOperations[ i ].args, -2.0f, 2.0f );
					BufferNeedsReset |= ImGui::IsItemEdited();
					break;
			}

			// color for this operation
			string colorLabel = string( "Color##" ) + std::to_string( i );
			ImGui::ColorEdit3( colorLabel.c_str(), ( float * ) &currentOperations[ i ].color, ImGuiColorEditFlags_PickerHueWheel );
			BufferNeedsReset |= ImGui::IsItemEdited();

			ImGui::Unindent();
		}
		// button to add another one after the list
		if ( ImGui::Button( " Add Random Operation " ) ) {
			currentOperations.push_back( GetRandomOperation() );
			BufferNeedsReset = true;
			RendererNeedsReset = true;
		}

		ImGui::End();
		// =================================

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

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			const GLuint shader = shaders[ "Draw" ];
			glUseProgram( shader );
			glUniform1f( glGetUniformLocation( shader, "brightness" ), brightness );
			glUniform1f( glGetUniformLocation( shader, "brightnessPower" ), brightnessPower );

			textureManager.BindImageForShader( "IFS Accumulator R", "ifsAccumulatorR", shader, 2 );
			textureManager.BindImageForShader( "IFS Accumulator G", "ifsAccumulatorG", shader, 3 );
			textureManager.BindImageForShader( "IFS Accumulator B", "ifsAccumulatorB", shader, 4 );

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

			if ( screenshotRequested ) {
				screenshotRequested = false;
				const GLuint tex = textureManager.Get( "Display Texture" );
				uvec2 dims = textureManager.GetDimensions( "Display Texture" );
				std::vector< float > imageBytesToSave;
				imageBytesToSave.resize( dims.x * dims.y * sizeof( float ) * 4, 0 );
				glBindTexture( GL_TEXTURE_2D, tex );
				glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
				Image_4F screenshot( dims.x, dims.y, &imageBytesToSave.data()[ 0 ] );
				screenshot.RGBtoSRGB();
				const string filename = string( "ifs-" ) + timeDateString() + string( ".png" );
				screenshot.Save( filename );
			}
		}

		{ // text rendering timestamp - required texture binds are handled internally
			scopedTimer Start( "Text Rendering" );
			textRenderer.Update( ImGui::GetIO().DeltaTime );
			textRenderer.Draw( textureManager.Get( "Display Texture" ) );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}

	void OnUpdate () {
		// application-specific update code
		ZoneScoped; scopedTimer Start( "Update" );

		// reset the buffer, when needed
		if ( RendererNeedsReset == true ) {
			RendererNeedsReset = false;

			// reset the value tracking the max
			constexpr uint32_t countValue = 0;
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, maxBuffer );
			glBufferData( GL_SHADER_STORAGE_BUFFER, 4, ( GLvoid * ) &countValue, GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, maxBuffer );

			// reset the accumulator
			const GLuint shader = shaders[ "Clear" ];
			glUseProgram( shader );

			textureManager.BindImageForShader( "IFS Accumulator R", "ifsAccumulatorR", shader, 2 );
			textureManager.BindImageForShader( "IFS Accumulator G", "ifsAccumulatorG", shader, 3 );
			textureManager.BindImageForShader( "IFS Accumulator B", "ifsAccumulatorB", shader, 4 );

			glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
			glMemoryBarrier( GL_ALL_BARRIER_BITS );
		}

		if ( BufferNeedsReset ) {
			BufferNeedsReset = false;
			PassOperationData();
		}

		const GLuint shader = shaders[ "Update" ];
		glUseProgram( shader );

		static rngi wangSeeder = rngi( 0, 10000000 );
		glUniform1i( glGetUniformLocation( shader, "wangSeed" ), wangSeeder() );
		glUniform1f( glGetUniformLocation( shader, "scale" ), scale );
		glUniform2f( glGetUniformLocation( shader, "offset" ), offset.x, offset.y );
		glUniform1i( glGetUniformLocation( shader, "numTransforms" ), currentOperations.size() );
		glUniform1i( glGetUniformLocation( shader, "initMode" ), initMode );

		// get the trident matrix
		mat3 tridentMatrix = mat3( trident.basisX, trident.basisY, trident.basisZ );
		glUniformMatrix3fv( glGetUniformLocation( shader, "tridentMatrix" ), 1, GL_FALSE, glm::value_ptr( tridentMatrix ) );

		textureManager.BindImageForShader( "IFS Accumulator R", "ifsAccumulatorR", shader, 2 );
		textureManager.BindImageForShader( "IFS Accumulator G", "ifsAccumulatorG", shader, 3 );
		textureManager.BindImageForShader( "IFS Accumulator B", "ifsAccumulatorB", shader, 4 );

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

		// event handling
		HandleCustomEvents();
		HandleQuitEvents();
		// trident for orientation
		HandleTridentEvents();
		if ( trident.Dirty() ) {
			RendererNeedsReset = true;
			trident.ClearDirty();
		}

		// derived-class-specific functionality
		OnUpdate();
		OnRender();

		FrameMark; // tells tracy that this is the end of a frame
		PrepareProfilingData(); // get profiling data ready for next frame
		return pQuit;
	}
};

int main ( int argc, char *argv[] ) {
	ifs engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

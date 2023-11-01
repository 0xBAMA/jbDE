#include "../../engine/engine.h"

struct aquariaConfig_t {
	ivec3 dimensions;
	int maxSpheres = 65535;

	float scale = 1.0f;

	// OpenGL Data
	GLuint sphereSSBO;
};

class Aquaria : public engineBase {	// example derived class
public:
	Aquaria () { Init(); OnInit(); PostInit(); }
	~Aquaria () { Quit(); }

	aquariaConfig_t aquariaConfig;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );
			cout << endl;

			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Dummy Draw" ] = computeShader( "./src/projects/Aquaria/shaders/dummyDraw.cs.glsl" ).shaderHandle;
			shaders[ "Precompute" ] = computeShader( "./src/projects/Aquaria/shaders/precompute.cs.glsl" ).shaderHandle;

	// ================================================================================================================
	// ==== Load Config ===============================================================================================
	// ================================================================================================================

			// get the configuration from config.json
			json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();
			aquariaConfig.dimensions.x	= j[ "app" ][ "Aquaria" ][ "dimensions" ][ "x" ];
			aquariaConfig.dimensions.y	= j[ "app" ][ "Aquaria" ][ "dimensions" ][ "y" ];
			aquariaConfig.dimensions.z	= j[ "app" ][ "Aquaria" ][ "dimensions" ][ "z" ];

	// ================================================================================================================
	// ==== Create Textures ===========================================================================================
	// ================================================================================================================
			// textureOptions_t opts;
			// opts.dataType		= GL_R32UI;
			// opts.width			= aquariaConfig.dimension.x;
			// opts.height			= aquariaConfig.dimension.y;
			// opts.textureType	= GL_TEXTURE_2D;
			// textureManager.Add( "Pheremone Continuum Buffer 0", opts );
			// textureManager.Add( "Pheremone Continuum Buffer 1", opts );

			textureOptions_t opts;
			opts.dataType		= GL_RG32UI;
			opts.textureType	= GL_TEXTURE_3D;
			opts.width			= aquariaConfig.dimensions.x;
			opts.height			= aquariaConfig.dimensions.y;
			opts.depth			= aquariaConfig.dimensions.z;
			textureManager.Add( "ID Buffer", opts );

			opts.dataType		= GL_RGBA16F;
			textureManager.Add( "Distance Buffer", opts );

	// ================================================================================================================
	// ===== Sphere Packing ===========================================================================================
	// ================================================================================================================

			// clear out the buffer
			const uint32_t padding = 0;
			const uint32_t maxSpheres = aquariaConfig.maxSpheres + padding; // 16-bit addressing gives us 65k max
			std::deque< vec4 > sphereLocationsPlusColors;

			// pick new palette, for the spheres
			palette::PickRandomPalette( true );

			// init the progress bar
			progressBar bar;
			bar.total = maxSpheres;
			bar.label = string( " Sphere Packing Compute: " );

			// stochastic sphere packing, inside the volume
			vec3 min = vec3( -aquariaConfig.dimensions.x / 2.0f, -aquariaConfig.dimensions.y / 2.0f, -aquariaConfig.dimensions.z / 2.0f );
			vec3 max = vec3(  aquariaConfig.dimensions.x / 2.0f,  aquariaConfig.dimensions.y / 2.0f,  aquariaConfig.dimensions.z / 2.0f );
			uint32_t maxIterations = 500;
			// hacky, following the pattern from before
			float currentRadius = 0.866f * aquariaConfig.dimensions.z / 2.0f;
			rng paletteRefVal = rng( 0.0f, 1.0f );
			rngN paletteRefJitter = rngN( 0.0f, 0.01f );
			float currentPaletteVal = paletteRefVal();

			while ( ( sphereLocationsPlusColors.size() / 2 ) < maxSpheres ) {
				rng x = rng( min.x + currentRadius, max.x - currentRadius );
				rng y = rng( min.y + currentRadius, max.y - currentRadius );
				rng z = rng( min.z + currentRadius, max.z - currentRadius );
				uint32_t iterations = maxIterations;
				// redundant check, but I think it's the easiest way not to run over
				while ( iterations-- && ( sphereLocationsPlusColors.size() / 2 ) < maxSpheres ) {
					// generate point inside the parent cube
					vec3 checkP = vec3( x(), y(), z() );
					// check for intersection against all other spheres
					bool foundIntersection = false;
					for ( uint idx = 0; idx < sphereLocationsPlusColors.size() / 2; idx++ ) {
						vec4 otherSphere = sphereLocationsPlusColors[ 2 * idx ];
						if ( glm::distance( checkP, otherSphere.xyz() ) < ( currentRadius + otherSphere.a ) ) {
							// cout << "intersection found in iteration " << iterations << endl;
							foundIntersection = true;
							break;
						}
					}
					// if there are no intersections, add it to the list with the current material
					if ( !foundIntersection ) {
						// cout << "adding sphere, " << checkP.x << " " << checkP.y << " " << checkP.z << " r: " << currentRadius << endl;
						// cout << "               " << currentMaterial.x << " " << currentMaterial.y << " " << currentMaterial.z << " r: " << currentRadius << endl;
						sphereLocationsPlusColors.push_back( vec4( checkP, currentRadius ) );
						sphereLocationsPlusColors.push_back( vec4( palette::paletteRef( std::clamp( currentPaletteVal + paletteRefJitter(), 0.0f, 1.0f ) ), 0.0f ) );

						// update and report
						bar.done = sphereLocationsPlusColors.size() / 2;
						if ( ( sphereLocationsPlusColors.size() / 2 ) % 50 == 0 || ( sphereLocationsPlusColors.size() / 2 ) == maxSpheres ) {
							bar.writeCurrentState();
						}
					}
				}
				// if you've gone max iterations, time to halve the radius and double the max iteration count, get new material
				currentPaletteVal = paletteRefVal();
				currentRadius /= 1.618f;
				maxIterations *= 3;

				// doing this makes it pack flat
				// min.y /= 1.5f;
				// max.y /= 1.5f;

				// slowly shrink bounds to accentuate the earlier placed spheres
				min *= 0.95f;
				max *= 0.95f;
			}

			// ================================================================================================================
			
			// send the SSBO
				// because it's a deque, I can pop the front N off ( see padding, above )
				// also, if I've got some kind of off by one issue, however I want to handle the zero reserve value, that'll be easy
			
			for ( uint32_t i = 0; i < padding; i++ ) {
				sphereLocationsPlusColors.pop_front();
			}

			std::vector< vec4 > vectorVersion;
			for ( auto& val : sphereLocationsPlusColors ) {
				cout << glm::to_string( val ) << endl;
				vectorVersion.push_back( val );
			}

			glGenBuffers( 1, &aquariaConfig.sphereSSBO );
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, aquariaConfig.sphereSSBO );
			glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * aquariaConfig.maxSpheres, ( GLvoid * ) &vectorVersion.data()[ 0 ], GL_DYNAMIC_COPY );
			// glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * aquariaConfig.maxSpheres, ( GLvoid * ) &sphereLocationsPlusColors[ 0 ], GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, aquariaConfig.sphereSSBO );
			
			// ================================================================================================================

			progressBar bar2;
			bar2.total = aquariaConfig.dimensions.z;
			cout << endl;
			bar2.label = string( " Data Cache Precompute:  " );

			// compute the distances / gradients / nearest IDs into the texture(s)
			// glUseProgram( shaders[ "Precompute" ] );

			// for ( int i = 0; i < aquariaConfig.dimensions.z; i++ ) {
			// 	glUseProgram( shaders[ "Precompute" ] );

			// 	// buffer setup
			// 	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, aquariaConfig.sphereSSBO );
			// 	textureManager.BindImageForShader( "ID Buffer", "idxBuffer", shaders[ "Precompute" ], 2 );
			// 	textureManager.BindImageForShader( "Distance Buffer", "dataCacheBuffer", shaders[ "Precompute" ], 3 );

			// 	// other uniforms
			// 	glUniform1i( glGetUniformLocation( shaders[ "Precompute" ], "slice" ), i );
			// 	glDispatchCompute( ( aquariaConfig.dimensions.x + 15 ) / 16, ( aquariaConfig.dimensions.y + 15 ) / 16, 1 );
			// 	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

			// 	// report current state
			// 	bar2.done = i + 1;
			// 	bar2.writeCurrentState();
			// 	// SDL_Delay( 10 );
			// }

			// ================================================================================================================

			cout << endl << endl;
		}

	// ================================================================================================================
	// ==== Physarum Init =============================================================================================
	// ================================================================================================================
		// agents, buffers
		// sage jenson - "guided" physarum - segments generated by L-system write values into the pheremone buffer? something like this

		// more:
			// vines, growing over the spheres
			// fishies?
			// forward PT ( caustics, lighting )

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
		// const bool control		= ( k & KMOD_CTRL );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

		if ( state[ SDL_SCANCODE_LEFTBRACKET ] )  { aquariaConfig.scale /= 0.99f; }
		if ( state[ SDL_SCANCODE_RIGHTBRACKET ] ) { aquariaConfig.scale *= 0.99f; }

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
			// static bool run = false;
			static int remaining = aquariaConfig.dimensions.z;
			// if ( !run ) {
			if ( remaining > 0 ) {
				// run = true;
				// progressBar bar;
				// bar.total = aquariaConfig.dimensions.z;
				// cout << endl;
				// bar.label = string( " Data Cache Precompute:  " );

				// for ( int i = 0; i < aquariaConfig.dimensions.z; i++ ) {
					remaining --;
					glUseProgram( shaders[ "Precompute" ] );

					// buffer setup
					glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, aquariaConfig.sphereSSBO );
					textureManager.BindImageForShader( "ID Buffer", "idxBuffer", shaders[ "Precompute" ], 2 );
					textureManager.BindImageForShader( "Distance Buffer", "dataCacheBuffer", shaders[ "Precompute" ], 3 );

					// other uniforms
					// glUniform1i( glGetUniformLocation( shaders[ "Precompute" ], "slice" ), i );
					glUniform1i( glGetUniformLocation( shaders[ "Precompute" ], "slice" ), remaining );
					glDispatchCompute( ( aquariaConfig.dimensions.x + 15 ) / 16, ( aquariaConfig.dimensions.y + 15 ) / 16, 1 );
					glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

					// report current state
					// bar.done = i + 1;
					// bar.writeCurrentState();
					// SDL_Delay( 10 );
				// }
				glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
				cout << endl;
			}
		}

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Dummy Draw" ] );
			textureManager.BindImageForShader( "ID Buffer", "idxBuffer", shaders[ "Dummy Draw" ], 2 );
			textureManager.BindImageForShader( "Distance Buffer", "dataCacheBuffer", shaders[ "Dummy Draw" ], 3 );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "time" ), SDL_GetTicks() / 1600.0f );

			const glm::mat3 inverseBasisMat = inverse( glm::mat3( -trident.basisX, -trident.basisY, -trident.basisZ ) );
			glUniformMatrix3fv( glGetUniformLocation( shaders[ "Dummy Draw" ], "invBasis" ), 1, false, glm::value_ptr( inverseBasisMat ) );
			glUniform3f( glGetUniformLocation( shaders[ "Dummy Draw" ], "blockSize" ), aquariaConfig.dimensions.x / 1024.0f,  aquariaConfig.dimensions.y / 1024.0f,  aquariaConfig.dimensions.z / 1024.0f );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "scale" ), aquariaConfig.scale );
			glUniform2f( glGetUniformLocation( shaders[ "Dummy Draw" ], "resolution" ), config.width, config.height );

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

		// { // show trident with current orientation
		// 	scopedTimer Start( "Trident" );
		// 	trident.Update( textureManager.Get( "Display Texture" ) );
		// 	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		// }
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

int main ( int argc, char *argv[] ) {
	Aquaria engineInstance;
	while( !engineInstance.MainLoop() );
	return 0;
}

#include "../../engine/engine.h"

class Aquaria : public engineBase {	// example derived class
public:
	Aquaria () { Init(); OnInit(); PostInit(); }
	~Aquaria () { Quit(); }

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );

			cout << endl;

			// something to put some basic data in the accumulator texture - specific to the demo project
			shaders[ "Dummy Draw" ] = computeShader( "./src/projects/Aquaria/shaders/dummyDraw.cs.glsl" ).shaderHandle;

	// ================================================================================================================
	// ==== Load Config ===============================================================================================
	// ================================================================================================================

	// ================================================================================================================
	// ===== Sphere Packing ===========================================================================================
	// ================================================================================================================

			// clear out the buffer
			const int maxSpheres = 65535; // 16-bit addressing
			std::vector< vec4 > sphereLocationsPlusColors;

			// pick new palette, for the spheres
			palette::PickRandomPalette( true );

			// init the progress bar
			progressBar bar;
			bar.total = maxSpheres;
			bar.label = string( " Sphere Packing: " );

			// stochastic sphere packing, inside the volume
			vec3 min = vec3( -8.0f, -1.5f, -8.0f );
			vec3 max = vec3(  8.0f,  1.5f,  8.0f );
			uint32_t maxIterations = 500;
			float currentRadius = 1.3f;
			rng paletteRefVal = rng( 0.0f, 1.0f );
			int material = 6;
			vec4 currentMaterial = vec4( palette::paletteRef( paletteRefVal() ), material );
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
						// cout << "adding sphere, " << currentRadius << endl;
						sphereLocationsPlusColors.push_back( vec4( checkP, currentRadius ) );
						sphereLocationsPlusColors.push_back( currentMaterial );

						// update and report
						bar.done = sphereLocationsPlusColors.size() / 2;
						if ( ( sphereLocationsPlusColors.size() / 2 ) % 50 == 0 || ( sphereLocationsPlusColors.size() / 2 ) == maxSpheres ) {
							bar.writeCurrentState();
						}
					}
				}
				// if you've gone max iterations, time to halve the radius and double the max iteration count, get new material
				currentMaterial = vec4( palette::paletteRef( paletteRefVal() ), material );
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
			// ================================================================================================================
				// compute the distances / gradients / nearest IDs into the texture(s)
			// ================================================================================================================

			cout << endl << endl;
		}

	// ================================================================================================================
	// ==== Physarum Init =============================================================================================
	// ================================================================================================================
		// agents, buffers

	// ================================================================================================================
	// ==== Fluid Sim Init - TBD ======================================================================================
	// ================================================================================================================

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

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Dummy Draw" ] );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "time" ), SDL_GetTicks() / 1600.0f );
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

		{ // show trident with current orientation
			scopedTimer Start( "Trident" );
			trident.Update( textureManager.Get( "Display Texture" ) );
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

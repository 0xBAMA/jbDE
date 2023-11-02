#include "../../engine/engine.h"

struct aquariaConfig_t {
	ivec3 dimensions;
	uint32_t maxSpheres = 65535;
	uint32_t sphereTrim;

	float scale = 1.0f;

	// for tiled update
	int deferredRemaining = 8 * 8 * 8;
	int lightingRemaining = 8 * 8 * 8;
	std::vector< ivec3 > offsets;

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
			shaders[ "Dummy Draw" ]	= computeShader( "./src/projects/Aquaria/shaders/dummyDraw.cs.glsl" ).shaderHandle;
			shaders[ "Precompute" ]	= computeShader( "./src/projects/Aquaria/shaders/precompute.cs.glsl" ).shaderHandle;
			shaders[ "Lighting" ] 	= computeShader( "./src/projects/Aquaria/shaders/lighting.cs.glsl" ).shaderHandle;

	// ================================================================================================================
	// ==== Load Config ===============================================================================================
	// ================================================================================================================

			// get the configuration from config.json
			json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();
			aquariaConfig.dimensions.x	= j[ "app" ][ "Aquaria" ][ "dimensions" ][ "x" ];
			aquariaConfig.dimensions.y	= j[ "app" ][ "Aquaria" ][ "dimensions" ][ "y" ];
			aquariaConfig.dimensions.z	= j[ "app" ][ "Aquaria" ][ "dimensions" ][ "z" ];
			aquariaConfig.sphereTrim	= j[ "app" ][ "Aquaria" ][ "sphereTrim" ];

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
			// textureManager.Add( "ID Buffer", opts );

			opts.dataType		= GL_RGBA16F;
			textureManager.Add( "Distance Buffer", opts );

	// ================================================================================================================
	// ===== Sphere Packing ===========================================================================================
	// ================================================================================================================

			glGenBuffers( 1, &aquariaConfig.sphereSSBO );
			ComputeUpdateOffsets();
			// ComputeSpherePacking();
			ComputePerlinPacking();

		}

	// ================================================================================================================
	// ==== Physarum Init =============================================================================================
	// ================================================================================================================
		// agents, buffers
		// sage jenson - "guided" physarum - segments generated by L-system write values into the pheremone buffer? something like this

		// more:
			// vines, growing over the spheres
			// fishies?
			// forward PT ( caustics from overhead water surface, other lighting )
				// some kind of feedback, plant growth informed by environmental lighting

	}

	void ComputeSpherePacking () {

		// clear out the buffer
		const uint32_t maxSpheres = aquariaConfig.maxSpheres + aquariaConfig.sphereTrim; // 16-bit addressing gives us 65k max
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
		rng alphaGen = rng( 0.5f, 1.0f );
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
					sphereLocationsPlusColors.push_back( vec4( checkP, currentRadius ) );
					sphereLocationsPlusColors.push_back( vec4( palette::paletteRef( std::clamp( currentPaletteVal + paletteRefJitter(), 0.0f, 1.0f ) ), alphaGen() ) );

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
			// min.z /= 1.25f;
			// max.z /= 1.25f;

			// slowly shrink bounds to accentuate the earlier placed spheres
			min *= 0.95f;
			max *= 0.95f;
		}

		// ================================================================================================================
		
		// send the SSBO
			// because it's a deque, I can pop the front N off ( see aquariaConfig.sphereTrim, above )
			// also, if I've got some kind of off by one issue, however I want to handle the zero reserve value, that'll be easy
		
		for ( uint32_t i = 0; i < aquariaConfig.sphereTrim; i++ ) {
			sphereLocationsPlusColors.pop_front();
		}

		std::vector< vec4 > vectorVersion;
		for ( auto& val : sphereLocationsPlusColors ) {
			vectorVersion.push_back( val );
		}

		glBindBuffer( GL_SHADER_STORAGE_BUFFER, aquariaConfig.sphereSSBO );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * aquariaConfig.maxSpheres, ( GLvoid * ) &vectorVersion.data()[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, aquariaConfig.sphereSSBO );

		cout << endl << endl;

		// ================================================================================================================
	}

	void ComputePerlinPacking () {

		// clear out the buffer
		const uint32_t maxSpheres = aquariaConfig.maxSpheres + aquariaConfig.sphereTrim; // 16-bit addressing gives us 65k max
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
		uint32_t maxIterations = 1000000;
		uint32_t iterations = maxIterations;

		// data generation
		rng alphaGen = rng( 0.5f, 1.0f );
		rng radiusJitter = rng( 0.1f, 1.0f );
		float padding = 20.0f;
		PerlinNoise p;

		while ( ( sphereLocationsPlusColors.size() / 2 ) < maxSpheres && iterations-- ) {
			rng x = rng( min.x + padding, max.x - padding );
			rng y = rng( min.y + padding, max.y - padding );
			rng z = rng( min.z + padding, max.z - padding );

			// generate point inside the parent cube
			vec3 checkP = vec3( x(), y(), z() );
			float noiseValue = p.noise( checkP.x / 130.0f, checkP.y / 130.0f, checkP.z / 130.0f );
			// float noiseValue = p.noise( checkP.x / 130.0f, checkP.y / 130.0f, 0.0f );

			if ( noiseValue > 0.75f ) {
				continue;
			}

			// determine radius, from the noise field
			float currentRadius = std::pow( radiusJitter(), 2.0f ) * RemapRange( std::pow( noiseValue, 5.0f ), 0.0f, 1.0f, 0.618f, 14.0f ) * aquariaConfig.dimensions.z / 24.0f;

			// check for intersection against all other spheres
			bool foundIntersection = false;
			for ( uint idx = 0; idx < sphereLocationsPlusColors.size() / 2; idx++ ) {
				vec4 otherSphere = sphereLocationsPlusColors[ 2 * idx ];
				if ( glm::distance( checkP, otherSphere.xyz() ) < ( currentRadius + otherSphere.a ) ) {
					foundIntersection = true;
					break;
				}
			}

			// if there are no intersections, add it to the list with the current material
			if ( !foundIntersection ) {
				sphereLocationsPlusColors.push_back( vec4( checkP, currentRadius ) );
				// sphereLocationsPlusColors.push_back( vec4( palette::paletteRef( std::clamp( noiseValue, 0.0f, 1.0f ) ), alphaGen() ) );
				sphereLocationsPlusColors.push_back( vec4( vec3( std::clamp( abs( ( noiseValue - 0.6f ) * 2.5f ), 0.0f, 1.0f ) ), alphaGen() ) );

				// update and report
				bar.done = sphereLocationsPlusColors.size() / 2;
				if ( ( sphereLocationsPlusColors.size() / 2 ) % 50 == 0 || ( sphereLocationsPlusColors.size() / 2 ) == maxSpheres ) {
					bar.writeCurrentState();
				}
			}
		}

		// ================================================================================================================
		
		// send the SSBO
			// because it's a deque, I can pop the front N off ( see aquariaConfig.sphereTrim, above )
			// also, if I've got some kind of off by one issue, however I want to handle the zero reserve value, that'll be easy
		
		for ( uint32_t i = 0; i < aquariaConfig.sphereTrim; i++ ) {
			sphereLocationsPlusColors.pop_front();
		}

		std::vector< vec4 > vectorVersion;
		for ( auto& val : sphereLocationsPlusColors ) {
			vectorVersion.push_back( val );
		}

		vectorVersion.resize( aquariaConfig.maxSpheres * 2 );

		glBindBuffer( GL_SHADER_STORAGE_BUFFER, aquariaConfig.sphereSSBO );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * aquariaConfig.maxSpheres, ( GLvoid * ) &vectorVersion.data()[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, aquariaConfig.sphereSSBO );

		cout << endl << endl;

		// ================================================================================================================
	}

	void ComputeUpdateOffsets () {
		aquariaConfig.offsets.clear();

		// generate the list of offsets via shuffle
		for ( int x = 0; x < 8; x++ )
		for ( int y = 0; y < 8; y++ )
		for ( int z = 0; z < 8; z++ ) {
			aquariaConfig.offsets.push_back( ivec3( x, y, z ) );
		}
		auto rng = std::default_random_engine {};
		std::shuffle( std::begin( aquariaConfig.offsets ), std::end( aquariaConfig.offsets ), rng );

		// manipulated bayer matrices... swap on 64's? that would be layer swaps
			/// bit of a pain in the ass getting that converted to 2d offsets
		// std::vector< uint8_t > dataSrc = BayerData( 8 );

	}

	void HandleCustomEvents () {
		// application specific controls
		ZoneScoped; scopedTimer Start( "HandleCustomEvents" );

		// // current state of the whole keyboard
		const uint8_t * state = SDL_GetKeyboardState( NULL );

		// // current state of the modifier keys
		const SDL_Keymod k		= SDL_GetModState();
		const bool shift		= ( k & KMOD_SHIFT );
		// const bool alt		= ( k & KMOD_ALT );
		// const bool control	= ( k & KMOD_CTRL );
		// const bool caps		= ( k & KMOD_CAPS );
		// const bool super		= ( k & KMOD_GUI );

		if ( state[ SDL_SCANCODE_LEFTBRACKET ] )  { aquariaConfig.scale /= shift ? 0.9f : 0.99f; }
		if ( state[ SDL_SCANCODE_RIGHTBRACKET ] ) { aquariaConfig.scale *= shift ? 0.9f : 0.99f; }

		if ( state[ SDL_SCANCODE_R ] && shift ) {
			ComputeUpdateOffsets();
			// ComputeSpherePacking();
			ComputePerlinPacking();
			aquariaConfig.deferredRemaining = 8 * 8 * 8;
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

		QuitConf( &quitConfirm ); // show quit confirm window, if triggered

		if ( showDemoWindow ) ImGui::ShowDemoWindow( &showDemoWindow );
	}

	void ComputePasses () {
		ZoneScoped;

		{ // dummy draw - draw something into accumulatorTexture
			scopedTimer Start( "Drawing" );
			bindSets[ "Drawing" ].apply();
			glUseProgram( shaders[ "Dummy Draw" ] );
			// textureManager.BindImageForShader( "ID Buffer", "idxBuffer", shaders[ "Dummy Draw" ], 2 );
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
		if ( aquariaConfig.deferredRemaining >= 0 ) {
			glUseProgram( shaders[ "Precompute" ] );

			// buffer setup
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, aquariaConfig.sphereSSBO );
			// textureManager.BindImageForShader( "ID Buffer", "idxBuffer", shaders[ "Precompute" ], 2 );
			textureManager.BindImageForShader( "Distance Buffer", "dataCacheBuffer", shaders[ "Precompute" ], 3 );

			// other uniforms
			ivec3 offset = aquariaConfig.offsets[ aquariaConfig.deferredRemaining-- ];
			glUniform3i( glGetUniformLocation( shaders[ "Precompute" ], "offset" ), offset.x, offset.y, offset.z );
			glDispatchCompute(
				( ( aquariaConfig.dimensions.x + 7 ) / 8 + 7 ) / 8,
				( ( aquariaConfig.dimensions.y + 7 ) / 8 + 7 ) / 8,
				( ( aquariaConfig.dimensions.z + 7 ) / 8 + 7 ) / 8 );
			glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

			if ( aquariaConfig.deferredRemaining == 0 ) {
				// block update finished, do lighting
				aquariaConfig.lightingRemaining = 8 * 8 * 8;
			}

		} else if ( aquariaConfig.lightingRemaining >= 0 ) {
			glUseProgram( shaders[ "Lighting" ] );

			// buffer setup
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, aquariaConfig.sphereSSBO );
			// textureManager.BindImageForShader( "ID Buffer", "idxBuffer", shaders[ "Lighting" ], 2 );
			textureManager.BindImageForShader( "Distance Buffer", "dataCacheBuffer", shaders[ "Lighting" ], 3 );

			// other uniforms
			ivec3 offset = aquariaConfig.offsets[ aquariaConfig.lightingRemaining-- ];
			glUniform3i( glGetUniformLocation( shaders[ "Lighting" ], "offset" ), offset.x, offset.y, offset.z );
			glDispatchCompute(
				( ( aquariaConfig.dimensions.x + 7 ) / 8 + 7 ) / 8,
				( ( aquariaConfig.dimensions.y + 7 ) / 8 + 7 ) / 8,
				( ( aquariaConfig.dimensions.z + 7 ) / 8 + 7 ) / 8 );
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

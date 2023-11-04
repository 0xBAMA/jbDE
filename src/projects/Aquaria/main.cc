#include "../../engine/engine.h"

struct spherePackConfig_t {
	// color picking
	float paletteRefMin = 0.0f;
	float paletteRefMax = 1.0f;
	float paletteRefJitter = 0.01f;
	float alphaGenMin = 0.5f;
	float alphaGenMax = 1.0f;

	// sizing
	float radiiInitialValue = 63.0f;
	float radiiStepShrink = 1.0f / 1.618f;
	float iterationMultiplier = 2.3f;

	// bounds manip
	vec3 boundsStepShrink = vec3( 0.99f, 0.99f, 0.92f );

	// termination
	uint32_t maxAllowedTotalIterations = 1000000;
	uint32_t sphereTrim = 100;
};

// struct perlinPackConfig_t {
	// TODO
// };

struct aquariaConfig_t {
	ivec3 dimensions;

	// leaving this global for now, because I still want to do the 16-bit addressing thing, at some point
	uint32_t maxSpheres = 65535;

	float scale = 1.0f;
	float thinLensIntensity = 0.1f;
	float thinLensDistance = 2.0f;
	float blendAmount = 0.9f;
	vec2 viewOffset = vec2( 0.0f );
	rngi wangSeeder = rngi( 0, 69420 );

	// worker thread/generation stuff
	int jobType = 0;
	bool workerThreadShouldRun = false;
	bool bufferReady = false;
	std::vector< vec4 > sphereBuffer;

	// config structs
	spherePackConfig_t pConfig;

	// scene setup
	bool refractiveBubble = false;
	float IoR = 4.0f;

	// volumetric effect
	float fogScalar = 0.0001618f;
	vec3 fogColor = vec3( 1.0f );

	// for tiled update
	std::vector< ivec3 > updateTiles;
	size_t numTiles;

	// for the bayer pattern analog thing, I prefer to do it this way
	int lightingRemaining = 0;
	std::vector< ivec3 > offsets;

	// OpenGL Data
	GLuint sphereSSBO;
};

class Aquaria : public engineBase {	// example derived class
public:
	Aquaria () { Init(); OnInit(); PostInit(); }
	~Aquaria () { Quit(); }

	aquariaConfig_t aquariaConfig;

	// for visually monitoring worker thread, GPU work, etc
	progressBar generateBar;
	progressBar evaluateBar;
	progressBar lightingBar;

	void OnInit () {
		ZoneScoped;
		{
			Block Start( "Additional User Init" );
			cout << endl;

			// prep reporter shit
			generateBar.reportWidth = evaluateBar.reportWidth = lightingBar.reportWidth = 16;
			generateBar.label = string( "Generate: " );
			evaluateBar.label = string( "Evaluate: " );
			lightingBar.label = string( "Lighting: " );

			// compile shaders
			CompileShaders();

			// get some initial palette selected
			palette::PickRandomPalette();

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
			// textureManager.Add( "ID Buffer", opts );

			opts.dataType		= GL_RGBA16F;
			textureManager.Add( "Distance Buffer", opts );

	// ================================================================================================================
	// ===== Sphere Packing ===========================================================================================
	// ================================================================================================================

			glGenBuffers( 1, &aquariaConfig.sphereSSBO );

			{
				// yucky, yucky, whatever
				ComputeTileList(); aquariaConfig.updateTiles.clear();
				ComputeUpdateOffsets(); aquariaConfig.offsets.clear();
			}

			// kick off worker thread, ready to go
			aquariaConfig.workerThreadShouldRun = true;

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

	void CompileShaders () {
		shaders[ "Dummy Draw" ]	= computeShader( "./src/projects/Aquaria/shaders/dummyDraw.cs.glsl" ).shaderHandle;
		shaders[ "Precompute" ]	= computeShader( "./src/projects/Aquaria/shaders/precompute.cs.glsl" ).shaderHandle;
		shaders[ "Lighting" ] 	= computeShader( "./src/projects/Aquaria/shaders/lighting.cs.glsl" ).shaderHandle;
	}

	void ComputeTileList () {
		aquariaConfig.updateTiles.clear();
		for ( int y = 0; y < aquariaConfig.dimensions.y; y += 64 ){
			for ( int x = 0; x < aquariaConfig.dimensions.x; x += 64 ){
				for ( int z = 0; z < aquariaConfig.dimensions.z; z += 64 ){
					aquariaConfig.updateTiles.push_back( ivec3( x, y, z ) );
				}
			}
		}
		evaluateBar.total = aquariaConfig.numTiles = aquariaConfig.updateTiles.size();
		// auto rng = std::default_random_engine {};
		// std::shuffle( std::begin( aquariaConfig.updateTiles ), std::end( aquariaConfig.updateTiles ), rng );
	}

	void ComputeSpherePacking () {
		const uint32_t maxSpheres = aquariaConfig.maxSpheres + aquariaConfig.pConfig.sphereTrim; // 16-bit addressing gives us 65k max
		std::deque< vec4 > sphereLocationsPlusColors;

		// stochastic sphere packing, inside the volume
		vec3 min = vec3( -aquariaConfig.dimensions.x / 2.0f, -aquariaConfig.dimensions.y / 2.0f, -aquariaConfig.dimensions.z / 2.0f );
		vec3 max = vec3(  aquariaConfig.dimensions.x / 2.0f,  aquariaConfig.dimensions.y / 2.0f,  aquariaConfig.dimensions.z / 2.0f );
		uint32_t maxIterations = 500;

		// I think this is the easiest way to handle things that don't terminate on their own
		uint32_t attemptsRemaining = aquariaConfig.pConfig.maxAllowedTotalIterations;

		// float currentRadius = 0.866f * aquariaConfig.dimensions.z / 2.0f;
		float currentRadius = aquariaConfig.pConfig.radiiInitialValue;
		rng paletteRefVal = rng( aquariaConfig.pConfig.paletteRefMin, aquariaConfig.pConfig.paletteRefMax );
		rng alphaGen = rng( aquariaConfig.pConfig.alphaGenMin, aquariaConfig.pConfig.alphaGenMax );
		rngN paletteRefJitter = rngN( 0.0f, aquariaConfig.pConfig.paletteRefJitter );
		float currentPaletteVal = paletteRefVal();

		while ( ( sphereLocationsPlusColors.size() / 2 ) < maxSpheres && !pQuit ) { // watch for program quit
			rng x = rng( min.x + currentRadius, max.x - currentRadius );
			rng y = rng( min.y + currentRadius, max.y - currentRadius );
			rng z = rng( min.z + currentRadius, max.z - currentRadius );
			uint32_t iterations = maxIterations;

			// redundant check, but I think it's the easiest way not to run over
			while ( iterations-- && ( sphereLocationsPlusColors.size() / 2 ) < maxSpheres && attemptsRemaining-- && !pQuit ) {
				// generate point inside the parent cube
				vec3 checkP = vec3( x(), y(), z() );
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
					sphereLocationsPlusColors.push_back( vec4( palette::paletteRef( std::clamp( currentPaletteVal + paletteRefJitter(), 0.0f, 1.0f ) ), alphaGen() ) );
				}

			// need to determine which is the greater percentage:
				// number of spheres / max spheres
				float sphereFrac = float( sphereLocationsPlusColors.size() / 2.0f ) / float( aquariaConfig.maxSpheres );
				// number of attempts taken as a fraction of max attempts
				float attemptFrac = float( aquariaConfig.pConfig.maxAllowedTotalIterations - attemptsRemaining ) / float( aquariaConfig.pConfig.maxAllowedTotalIterations );

				// the greater of the two should set the level on the progress bar
				generateBar.done = std::max( sphereFrac, attemptFrac );
				generateBar.total = 1.0f;

				if ( !attemptsRemaining ) break;
			}
			if ( !attemptsRemaining ) break;

			// if you've gone max iterations, time to halve the radius and double the max iteration count, get new material
			currentPaletteVal = paletteRefVal();
			currentRadius *= aquariaConfig.pConfig.radiiStepShrink;
			maxIterations *= aquariaConfig.pConfig.iterationMultiplier;

			// this replaces explicit shrinking on each axis
			min.x *= aquariaConfig.pConfig.boundsStepShrink.x; max.x *= aquariaConfig.pConfig.boundsStepShrink.x;
			min.y *= aquariaConfig.pConfig.boundsStepShrink.y; max.y *= aquariaConfig.pConfig.boundsStepShrink.y;
			min.z *= aquariaConfig.pConfig.boundsStepShrink.z; max.z *= aquariaConfig.pConfig.boundsStepShrink.z;

		}

		// ================================================================================================================

		// because it's a deque, I can pop the front N off ( see aquariaConfig.sphereTrim, above )
		// also, if I've got some kind of off by one issue, however I want to handle the zero reserve value, that'll be easy

		for ( uint32_t i = 0; i < aquariaConfig.pConfig.sphereTrim; i++ ) {
			sphereLocationsPlusColors.pop_front();
		}

		aquariaConfig.sphereBuffer.resize( 0 );
		aquariaConfig.sphereBuffer.reserve( sphereLocationsPlusColors.size() );
		for ( auto& val : sphereLocationsPlusColors ) {
			aquariaConfig.sphereBuffer.push_back( val );
		}

		// if we aborted from running out of iterations, avoid seg fault in glBufferData
		aquariaConfig.sphereBuffer.resize( aquariaConfig.maxSpheres * 2 );

			// make sure that the generate progress bar reports 100% at this stage

		// ================================================================================================================
	}

	void ComputePerlinPacking () {
		// const uint32_t maxSpheres = aquariaConfig.maxSpheres + aquariaConfig.sphereTrim; // 16-bit addressing gives us 65k max
		const uint32_t maxSpheres = aquariaConfig.maxSpheres; // 16-bit addressing gives us 65k max
		std::deque< vec4 > sphereLocationsPlusColors;

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

			checkP.z *= RemapRange( noiseValue, 0.0f, 1.0f, 0.25f, 1.2f );

			// if ( noiseValue > 0.75f ) {
				// continue;
			// }

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
				// sphereLocationsPlusColors.push_back( vec4( vec3( std::clamp( abs( ( noiseValue - 0.6f ) * 2.5f ), 0.0f, 1.0f ) ), alphaGen() ) );
				sphereLocationsPlusColors.push_back( vec4( palette::paletteRef( std::clamp( noiseValue, 0.0f, 1.0f ) ), noiseValue ) );

				// update and report
				bar.done = sphereLocationsPlusColors.size() / 2;
				if ( ( sphereLocationsPlusColors.size() / 2 ) % 50 == 0 || ( sphereLocationsPlusColors.size() / 2 ) == maxSpheres ) {
					bar.writeCurrentState();
				}
			}
		}

		// ================================================================================================================
		
		// send the SSBO
		std::vector< vec4 > vectorVersion;
		for ( auto& val : sphereLocationsPlusColors ) {
			vectorVersion.push_back( val );
		}
		// pad out the rest of the buffer, if we had an early termination
		vectorVersion.resize( aquariaConfig.maxSpheres * 2 );

		glBindBuffer( GL_SHADER_STORAGE_BUFFER, aquariaConfig.sphereSSBO );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * aquariaConfig.maxSpheres, ( GLvoid * ) &vectorVersion.data()[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, aquariaConfig.sphereSSBO );

		// ================================================================================================================
	}

	void ComputeUpdateOffsets () {
		aquariaConfig.offsets.clear();
		aquariaConfig.lightingRemaining = lightingBar.total = 8 * 8 * 8;

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

		// zoom in and out
		if ( state[ SDL_SCANCODE_LEFTBRACKET ] )  { aquariaConfig.scale /= shift ? 0.9f : 0.99f; }
		if ( state[ SDL_SCANCODE_RIGHTBRACKET ] ) { aquariaConfig.scale *= shift ? 0.9f : 0.99f; }

		// vim-style x,y offsetting
		if ( state[ SDL_SCANCODE_H ] ) { aquariaConfig.viewOffset.x += shift ? 5.0f : 1.0f; }
		if ( state[ SDL_SCANCODE_J ] ) { aquariaConfig.viewOffset.y -= shift ? 5.0f : 1.0f; }
		if ( state[ SDL_SCANCODE_K ] ) { aquariaConfig.viewOffset.y += shift ? 5.0f : 1.0f; }
		if ( state[ SDL_SCANCODE_L ] ) { aquariaConfig.viewOffset.x -= shift ? 5.0f : 1.0f; }

		// hot recompile
		if ( state[ SDL_SCANCODE_Y ] ) {
			CompileShaders();
		}
	}

	void ControlsWindow () {
		ImGui::Begin( "Controls", NULL, 0 );
		if ( ImGui::BeginTabBar( "Config Sections", ImGuiTabBarFlags_None ) ) {
			if ( ImGui::BeginTabItem( "Generation" ) ) {
				// prototype palette browser
				ImGui::SeparatorText( "Palette Browser" );
				std::vector< const char * > paletteLabels;
				if ( paletteLabels.size() == 0 ) {
					for ( auto& entry : palette::paletteListLocal ) {
						// copy to a cstr for use by imgui
						char * d = new char[ entry.label.length() + 1 ];
						std::copy( entry.label.begin(), entry.label.end(), d );
						d[ entry.label.length() ] = '\0';
						paletteLabels.push_back( d );
					}
				}

				ImGui::Combo( "Palette", &palette::PaletteIndex, paletteLabels.data(), paletteLabels.size() );
				ImGui::SameLine();
				if ( ImGui::Button( "Random" ) ) {
					palette::PickRandomPalette( false );
				}
				const size_t paletteSize = palette::paletteListLocal[ palette::PaletteIndex ].colors.size();
				ImGui::Text( "  Contains %.3lu colors:", palette::paletteListLocal[ palette::PaletteIndex ].colors.size() );
				// handle max < min
				float realSelectedMin = std::min( aquariaConfig.pConfig.paletteRefMin, aquariaConfig.pConfig.paletteRefMax );
				float realSelectedMax = std::max( aquariaConfig.pConfig.paletteRefMin, aquariaConfig.pConfig.paletteRefMax );
				size_t minShownIdx = std::floor( realSelectedMin * ( paletteSize - 1 ) );
				size_t maxShownIdx = std::ceil( realSelectedMax * ( paletteSize - 1 ) );

				bool finished = false;
				for ( int y = 0; y < 8; y++ ) {
					ImGui::Text( " " );
					for ( int x = 0; x < 32; x++ ) {

						// terminate when you run out of colors
						const uint index = x + 32 * y;
						if ( index >= paletteSize ) {
							finished = true;
						}

						// show color, or black if past the end of the list
						ivec4 color = ivec4( 0 );
						if ( !finished ) {
							color = ivec4( palette::paletteListLocal[ palette::PaletteIndex ].colors[ index ], 255 );
							// determine if it is in the active range
							if ( index < minShownIdx || index > maxShownIdx ) {
								color.a = 64; // dim inactive entries
							}
						} 
						ImGui::SameLine();
						ImGui::TextColored( ImVec4( color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f ), "@" );

					}
				}

				ImGui::SeparatorText( "Sphere Packing Config" );
				// build config struct, pass to the functions
					// perlin or incremental version

				ImGui::Text( " Method: " );
				ImGui::SameLine();
				ImGui::RadioButton( " Incremental ", &aquariaConfig.jobType, 0 );
				ImGui::SameLine();
				ImGui::RadioButton( " Perlin ", &aquariaConfig.jobType, 1 );

				// only show the controls for one at a time, to read easier
				if ( aquariaConfig.jobType == 0 ) {	// incremental version
					// manipulate values
					ImGui::SliderFloat( "Palette Min", &aquariaConfig.pConfig.paletteRefMin, 0.0f, 1.0f );
					ImGui::SliderFloat( "Palette Max", &aquariaConfig.pConfig.paletteRefMax, 0.0f, 1.0f );
					ImGui::SliderFloat( "Palette Jitter", &aquariaConfig.pConfig.paletteRefJitter, 0.0f, 0.2f );

					ImGui::Text( " " );

					ImGui::SliderFloat( "Alpha Min", &aquariaConfig.pConfig.alphaGenMin, 0.0f, 1.0f );
					ImGui::SliderFloat( "Alpha Max", &aquariaConfig.pConfig.alphaGenMax, 0.0f, 1.0f );

					ImGui::Text( " " );

					ImGui::SliderFloat( "Initial Radius", &aquariaConfig.pConfig.radiiInitialValue, 1.0f, 300.0f );
					ImGui::SliderFloat( "Radius Step Shrink", &aquariaConfig.pConfig.radiiStepShrink, 0.0f, 1.0f );
					ImGui::SliderFloat( "Iteration Count Multiplier", &aquariaConfig.pConfig.iterationMultiplier, 0.0f, 10.0f );
					ImGui::SliderInt( "Max Total Iterations", ( int * ) &aquariaConfig.pConfig.maxAllowedTotalIterations, 0, 100000000 );

					ImGui::Text( " " );

					ImGui::SliderFloat( "X Bounds Step Shrink", &aquariaConfig.pConfig.boundsStepShrink.x, 0.0f, 1.0f );
					ImGui::SliderFloat( "Y Bounds Step Shrink", &aquariaConfig.pConfig.boundsStepShrink.y, 0.0f, 1.0f );
					ImGui::SliderFloat( "Z Bounds Step Shrink", &aquariaConfig.pConfig.boundsStepShrink.z, 0.0f, 1.0f );

					ImGui::Text( " " );
					ImGui::SliderInt( "Trim", ( int* ) &aquariaConfig.pConfig.sphereTrim, 0, 100000 );

					if ( ImGui::Button( " Do It " ) ) {
						// worker thread sees this and begins work
						aquariaConfig.workerThreadShouldRun = true;
					}
				} else if ( aquariaConfig.jobType == 1 ) { // perlin mode

				}
				ImGui::EndTabItem();
			}
			if ( ImGui::BeginTabItem( "Rendering" ) ) {
				// controlling things like fog density, fog color
				// lighting controls, button to recompute lighting
					// or flag that gets set when a value changes with ImGui::IsItemEdited() etc

				ImGui::SeparatorText( " Thin Lens " );
				ImGui::SliderFloat( "Intensity", &aquariaConfig.thinLensIntensity, 0.0f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic );
				ImGui::SliderFloat( "Distance", &aquariaConfig.thinLensDistance, 0.0f, 5.0f, "%.3f" );
				ImGui::SeparatorText( " Scene " );
				ImGui::Checkbox( "Bubble", &aquariaConfig.refractiveBubble );
				ImGui::SliderFloat( "Bubble IoR", &aquariaConfig.IoR, -6.0f, 6.0f );
				ImGui::SliderFloat( "Fog Scalar", &aquariaConfig.fogScalar, -0.001f, 0.001f, "%.6f", ImGuiSliderFlags_Logarithmic );
				ImGui::ColorEdit3( "Fog Color", ( float * ) &aquariaConfig.fogColor, ImGuiColorEditFlags_PickerHueWheel );
				ImGui::SeparatorText( " Tonemapping " );
				const char* tonemapModesList[] = {
					"None (Linear)",
					"ACES (Narkowicz 2015)",
					"Unreal Engine 3",
					"Unreal Engine 4",
					"Uncharted 2",
					"Gran Turismo",
					"Modified Gran Turismo",
					"Rienhard",
					"Modified Rienhard",
					"jt",
					"robobo1221s",
					"robo",
					"reinhardRobo",
					"jodieRobo",
					"jodieRobo2",
					"jodieReinhard",
					"jodieReinhard2"
				};
				ImGui::Combo("Tonemapping Mode", &tonemap.tonemapMode, tonemapModesList, IM_ARRAYSIZE( tonemapModesList ) );
				ImGui::SliderFloat( "Gamma", &tonemap.gamma, 0.0f, 3.0f );
				ImGui::SliderFloat( "PostExposure", &tonemap.postExposure, 0.0f, 5.0f );
				ImGui::SliderFloat( "Saturation", &tonemap.saturation, 0.0f, 4.0f );
				// ImGui::Checkbox( "Saturation Uses Improved Weight Vector", &tonemap.saturationImprovedWeights );
				ImGui::SliderFloat( "Color Temperature", &tonemap.colorTemp, 1000.0f, 40000.0f );
				if ( ImGui::Button( "Reset to Defaults" ) ) {
					TonemapDefaults();
				}
				ImGui::SeparatorText( " Other Rendering " );
				ImGui::SliderFloat( "Blend Amount", &aquariaConfig.blendAmount, 0.9f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic );
				
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();
	}

	void ImguiPass () {
		ZoneScoped;
		if ( showProfiler ) {
			static ImGuiUtils::ProfilersWindow profilerWindow; // add new profiling data and render
			profilerWindow.cpuGraph.LoadFrameData( &tasks_CPU[ 0 ], tasks_CPU.size() );
			profilerWindow.gpuGraph.LoadFrameData( &tasks_GPU[ 0 ], tasks_GPU.size() );
			profilerWindow.Render(); // GPU graph is presented on top, CPU on bottom
		}

		ControlsWindow();

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

			static rng blueNoiseOffset = rng( 0, 512 );
			glUniform2i( glGetUniformLocation( shaders[ "Dummy Draw" ], "noiseOffset" ), blueNoiseOffset(), blueNoiseOffset() );

			const glm::mat3 inverseBasisMat = inverse( glm::mat3( -trident.basisX, -trident.basisY, -trident.basisZ ) );
			glUniformMatrix3fv( glGetUniformLocation( shaders[ "Dummy Draw" ], "invBasis" ), 1, false, glm::value_ptr( inverseBasisMat ) );
			glUniform3f( glGetUniformLocation( shaders[ "Dummy Draw" ], "blockSize" ), aquariaConfig.dimensions.x / 1024.0f,  aquariaConfig.dimensions.y / 1024.0f,  aquariaConfig.dimensions.z / 1024.0f );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "scale" ), aquariaConfig.scale );
			glUniform2f( glGetUniformLocation( shaders[ "Dummy Draw" ], "viewOffset" ), aquariaConfig.viewOffset.x, aquariaConfig.viewOffset.y );
			glUniform1i( glGetUniformLocation( shaders[ "Dummy Draw" ], "wangSeed" ), aquariaConfig.wangSeeder() );
			glUniform2f( glGetUniformLocation( shaders[ "Dummy Draw" ], "resolution" ), config.width, config.height );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "blendAmount" ), aquariaConfig.blendAmount );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "thinLensDistance" ), aquariaConfig.thinLensDistance );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "thinLensIntensity" ), aquariaConfig.thinLensIntensity );
			glUniform1i( glGetUniformLocation( shaders[ "Dummy Draw" ], "refractiveBubble" ), aquariaConfig.refractiveBubble );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "bubbleIoR" ), aquariaConfig.IoR );
			glUniform1f( glGetUniformLocation( shaders[ "Dummy Draw" ], "fogScalar" ), aquariaConfig.fogScalar );
			glUniform3f( glGetUniformLocation( shaders[ "Dummy Draw" ], "fogColor" ), aquariaConfig.fogColor.r, aquariaConfig.fogColor.g, aquariaConfig.fogColor.b );

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

			// get status from engine local progress bars, write them above the timestamp
				// updated from worker thread, so we don't lock up like before

			textRenderer.DrawProgressBarString( 3, generateBar );
			textRenderer.DrawProgressBarString( 2, evaluateBar );
			textRenderer.DrawProgressBarString( 1, lightingBar );

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

	// update thread
	std::thread workerThread { [=] () {
		while ( !pQuit ) { // while the program is running
			if ( !aquariaConfig.workerThreadShouldRun ) { // waiting
				std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
			} else {

			// 	auto tstart = std::chrono::high_resolution_clock::now();

				// reset the bars
				generateBar.done = evaluateBar.done = lightingBar.done = 0.0f;

				if ( aquariaConfig.jobType == 0 ) { // incremental version
					ComputeSpherePacking();
				} else if ( aquariaConfig.jobType == 1 ) {

				}

			// 	float msLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(
			// 		std::chrono::high_resolution_clock::now() - tstart ).count();

				// and the data is ready
				aquariaConfig.bufferReady = true;
				aquariaConfig.workerThreadShouldRun = false;
			}
		}
	}};

	void OnUpdate () {
		ZoneScoped; scopedTimer Start( "Update" );

		// generate bar is updated internal to the generate functions - this is hacky but whatever
		if ( aquariaConfig.workerThreadShouldRun || aquariaConfig.bufferReady ) {
			evaluateBar.done = lightingBar.done= 0.0f;
		} else {
			evaluateBar.done = aquariaConfig.numTiles - aquariaConfig.updateTiles.size();
			lightingBar.done = 8 * 8 * 8 - aquariaConfig.lightingRemaining;
		}

		if ( aquariaConfig.bufferReady ) {

			// update state
			aquariaConfig.bufferReady = false;
			aquariaConfig.workerThreadShouldRun = false;

			// send the data to the SSBO
			glBindBuffer( GL_SHADER_STORAGE_BUFFER, aquariaConfig.sphereSSBO );
			glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * aquariaConfig.maxSpheres, ( GLvoid * ) &aquariaConfig.sphereBuffer.data()[ 0 ], GL_DYNAMIC_COPY );
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, aquariaConfig.sphereSSBO );

			ComputeTileList(); // ready to go to the next stage
			ComputeUpdateOffsets();

		} else if ( aquariaConfig.updateTiles.size() != 0 ) {
			glUseProgram( shaders[ "Precompute" ] );

			// buffer setup
			glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, aquariaConfig.sphereSSBO );
			// textureManager.BindImageForShader( "ID Buffer", "idxBuffer", shaders[ "Precompute" ], 2 );
			textureManager.BindImageForShader( "Distance Buffer", "dataCacheBuffer", shaders[ "Precompute" ], 3 );

			// other uniforms
			ivec3 offset = aquariaConfig.updateTiles[ aquariaConfig.updateTiles.size() - 1 ];
			aquariaConfig.updateTiles.pop_back();
			glUniform3i( glGetUniformLocation( shaders[ "Precompute" ], "offset" ), offset.x, offset.y, offset.z );
			glDispatchCompute( 8, 8, 8 );
			// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

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

			// do a second one
			if ( aquariaConfig.lightingRemaining >= 0 && !( aquariaConfig.workerThreadShouldRun || aquariaConfig.bufferReady ) ) {
				// other uniforms
				offset = aquariaConfig.offsets[ aquariaConfig.lightingRemaining-- ];
				glUniform3i( glGetUniformLocation( shaders[ "Lighting" ], "offset" ), offset.x, offset.y, offset.z );
				glDispatchCompute(
					( ( aquariaConfig.dimensions.x + 7 ) / 8 + 7 ) / 8,
					( ( aquariaConfig.dimensions.y + 7 ) / 8 + 7 ) / 8,
					( ( aquariaConfig.dimensions.z + 7 ) / 8 + 7 ) / 8 );
			}
			// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
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

	// program has terminated, time to die
	engineInstance.workerThread.join();

	return 0;
}

struct icarusState_t {
	// access to the texture manager
	textureManager_t * textureManager;

	// shaders
	GLuint CopyShader;
	GLuint AdamShader;
	GLuint PrepShader;
	GLuint DrawShader;
	GLuint ClearShader;

	// first pass on the pipeline
	GLuint RayClearShader;
	GLuint RayGenerateShader;
	GLuint RayIntersectShader_SDF;
	GLuint RayIntersectShader_Triangle;
	GLuint RayIntersectShader_TinyBVH;
	GLuint RayIntersectShader_Volume;
	GLuint RayShadeShader;
	GLuint WipeShader;

	// BVH management
	SoftRast modelLoader;
	tinybvh::BVH bvh;
	tinybvh::bvhvec4 *vertices;

	// dimensions and related info
	// uvec2 dimensions = uvec2( 1280, 720 );
	uvec2 dimensions = uvec2( 1920, 1080 );
	int numLevels;

	// perf settings
	const int numIntersectors = 4; // I'd rather get this straight from the header...
	uint maxBounces		= 16;
	bool runSDF			= true;
	bool runTriangle	= false;
	bool runVolume		= false;

	// how are we generating primary ray locations
	#define UNIFORM		0
	#define GAUSSIAN	1
	#define SHUFFLED	2

	int offsetFeedMode = SHUFFLED;
	GLuint offsetsSSBO;
	GLuint intersectionScratchSSBO;
	bool forceUpdate = false;

	uint numRays = 2 << 16;

	// holding the rays
	GLuint raySSBO;

	// camera parameterization
	vec3 viewerPosition = vec3( 0.0f );
	vec3 basisX = vec3( 1.0f, 0.0f, 0.0f );
	vec3 basisY = vec3( 0.0f, 1.0f, 0.0f );
	vec3 basisZ = vec3( 0.0f, 0.0f, 1.0f );
	float FoV = 0.618f;
	int cameraMode = 0;
	float uvScaleFactor = 1.0f;
	int DoFBokehMode = 0;
	float DoFRadius = 0.001f;
	float DoFFocusDistance = 5.0f;
	float chromabScaleFactor = 0.006f;
};

void LoadBVH ( icarusState_t &state ) {
// this works, loads 200k triangles - stanford tyranosaurus - good test model for the BVH stuff
	state.modelLoader.LoadModel( "../tyra.obj", "../" );
	cout << endl << "Model has " << state.modelLoader.triangles.size() << " tris" << endl;

	// put it in some known span
	state.modelLoader.UnitCubeRefit();

	// allocate space for that many verts
	state.vertices = ( tinybvh::bvhvec4 * ) malloc( 3 * state.modelLoader.triangles.size() * sizeof( tinybvh::bvhvec4 ) );

	// copy from the loader to the bvh's list
	for ( size_t i = 0; i < state.modelLoader.triangles.size(); i++ ) {
		const int baseIdx = 3 * i;

		// add the triangles to the BVH
		state.vertices[ baseIdx + 0 ].x = state.modelLoader.triangles[ i ].p0.x;
		state.vertices[ baseIdx + 0 ].y = state.modelLoader.triangles[ i ].p0.y;
		state.vertices[ baseIdx + 0 ].z = state.modelLoader.triangles[ i ].p0.z;

		state.vertices[ baseIdx + 1 ].x = state.modelLoader.triangles[ i ].p1.x;
		state.vertices[ baseIdx + 1 ].y = state.modelLoader.triangles[ i ].p1.y;
		state.vertices[ baseIdx + 1 ].z = state.modelLoader.triangles[ i ].p1.z;

		state.vertices[ baseIdx + 2 ].x = state.modelLoader.triangles[ i ].p2.x;
		state.vertices[ baseIdx + 2 ].y = state.modelLoader.triangles[ i ].p2.y;
		state.vertices[ baseIdx + 2 ].z = state.modelLoader.triangles[ i ].p2.z;
	}

	// build the bvh from the list of triangles
	state.bvh.Build( state.vertices, state.modelLoader.triangles.size() );
	state.bvh.Convert( tinybvh::BVH::WALD_32BYTE, tinybvh::BVH::VERBOSE );
	state.bvh.Refit( tinybvh::BVH::VERBOSE );

	// testing rays against it
	Image_4U test( 5000, 5000 );
	for ( uint32_t y = 0; y < test.Height(); y++ ) {
		for ( uint32_t x = 0; x < test.Width(); x++ ) {
			color_4U col;

			// tinybvh::bvhvec3 O( RangeRemap( x + 0.5f, 0.0f, test.Width(), -1.0f, 1.0f ), RangeRemap( y + 0.5f, 0.0f, test.Height(), -1.0f, 1.0f ), 1.0f );
			tinybvh::bvhvec3 O( 1.0f, RangeRemap( y + 0.5f, 0.0f, test.Width(), -1.0f, 1.0f ), RangeRemap( x + 0.5f, 0.0f, test.Height(), -1.0f, 1.0f ) );
			tinybvh::bvhvec3 D( -1.0f, 0.0f, 0.0f );
			tinybvh::Ray ray( O, D );
			uint steps = uint( state.bvh.Intersect( ray ) );
			// printf( "%d %d: nearest intersection: %f (found in %i traversal steps).\n", x, y, ray.hit.t, steps );

			// if ( ray.hit.t < 1e5f ) {
			// 	// cout << "hit" << endl;
			// 	col = color_4U( { ( uint8_t ) ( 255 * ( ray.hit.t / 2.0f ) ), ( uint8_t ) steps * 3u, ( uint8_t ) 0, ( uint8_t ) 255 } );
			// } else {
			// 	col = color_4U( { ( uint8_t ) 16, ( uint8_t ) steps * 3u, ( uint8_t ) 0, ( uint8_t ) 255 } );
			// }
			col = color_4U( { ( uint8_t ) ( ( ray.hit.t < 1e5f ) ? ( 255 * ( ray.hit.t / 2.0f ) ) : steps * 3u ), ( uint8_t ) steps * 3u, ( uint8_t ) 0, 255 } );
			test.SetAtXY( x, y, col );
		}
	}

	test.Save( "test.png" );

	// create the three buffers:
		// GPU nodes
		// index data
		// triangle data
}

// =============================================================================================================
// Initialization
void CompileShaders ( icarusState_t &state ) {

	const string basePath = string( "./src/projects/PathTracing/Icarus/shaders/" );

	state.CopyShader 			= computeShader( basePath + "copy.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.CopyShader, -1, string( "Copy" ).c_str() );

	state.AdamShader			= computeShader( basePath + "adam.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.AdamShader, -1, string( "Adam" ).c_str() );

	state.PrepShader			= computeShader( basePath + "prep.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.PrepShader, -1, string( "Prep" ).c_str() );

	state.DrawShader			= computeShader( basePath + "draw.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.DrawShader, -1, string( "Draw" ).c_str() );

	state.ClearShader			= computeShader( basePath + "clear.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.ClearShader, -1, string( "Clear" ).c_str() );


	// ray generation, intersection, and shading
	state.RayClearShader				= computeShader( basePath + "rayClear.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.RayClearShader, -1, string( "Ray Clear" ).c_str() );

	state.RayGenerateShader				= computeShader( basePath + "rayGenerate.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.RayGenerateShader, -1, string( "Ray Generate" ).c_str() );

	state.RayIntersectShader_SDF		= computeShader( basePath + "rayIntersect_SDF.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.RayIntersectShader_SDF, -1, string( "Ray Intersect (SDF)" ).c_str() );

	state.RayIntersectShader_Triangle	= computeShader( basePath + "rayIntersect_Triangle.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.RayIntersectShader_Triangle, -1, string( "Ray Intersect (Triangle)" ).c_str() );

	state.RayIntersectShader_TinyBVH	= computeShader( basePath + "rayIntersect_TinyBVH.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.RayIntersectShader_TinyBVH, -1, string( "Ray Intersect (TinyBVH)" ).c_str() );

	state.RayIntersectShader_Volume		= computeShader( basePath + "rayIntersect_Volume.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.RayIntersectShader_Volume, -1, string( "Ray Intersect (Volume)" ).c_str() );

	state.RayShadeShader				= computeShader( basePath + "rayShade.cs.glsl" ).shaderHandle;
	glObjectLabel( GL_PROGRAM, state.RayShadeShader, -1, string( "Ray Shade/Bounce" ).c_str() );

}

void AllocateBuffers ( icarusState_t &state ) {
	// allocate the ray buffer
	glGenBuffers( 1, &state.offsetsSSBO );		// pixel offsets
	glGenBuffers( 1, &state.raySSBO );		// ray state front and back buffer(s)
	glGenBuffers( 1, &state.intersectionScratchSSBO ); // intersection structs

	// allocate space for ray offsets - 2x ints * state.numRays offsets per frame
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, state.offsetsSSBO );
	glBufferData( GL_SHADER_STORAGE_BUFFER, 2 * sizeof( GLuint ) * state.numRays, NULL, GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, state.offsetsSSBO );
	glObjectLabel( GL_BUFFER, state.offsetsSSBO, -1, string( "Pixel Offsets" ).c_str() );

	// allocate space for the ray state structs, state.numRays of them - this is going to change a lot, as I figure out what needs to happen
		// currently 64 bytes, see rayState.h.glsl
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, state.raySSBO );
	glBufferData( GL_SHADER_STORAGE_BUFFER, 64 * state.numRays, NULL, GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, state.raySSBO );
	glObjectLabel( GL_BUFFER, state.raySSBO, -1, string( "Ray State Buffer" ).c_str() );

	// scratch memory for the ray state structs
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, state.intersectionScratchSSBO );
	glBufferData( GL_SHADER_STORAGE_BUFFER, 64 * state.numIntersectors * state.numRays, NULL, GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, state.intersectionScratchSSBO );
	glObjectLabel( GL_BUFFER, state.intersectionScratchSSBO, -1, string( "Intersection Buffer" ).c_str() );

	// buffers for the BVH
	LoadBVH( state );
}

void AllocateTextures ( icarusState_t &state ) {
	static bool firstTime = true;
	if ( !firstTime ) {
		// todo: delete the existing textures, first
		state.textureManager->Remove( "Output Buffer" );
		state.textureManager->Remove( "R Tally Image" );
		state.textureManager->Remove( "G Tally Image" );
		state.textureManager->Remove( "B Tally Image" );
		state.textureManager->Remove( "Sample Count" );
		state.textureManager->Remove( "Adam" );
	}
	firstTime = false;

	textureOptions_t opts;

	// image to hold tonemapped results
	opts.width			= state.dimensions.x;
	opts.height			= state.dimensions.y;
	opts.dataType		= GL_RGBA32F;
	opts.minFilter		= GL_LINEAR;
	opts.magFilter		= GL_LINEAR;
	opts.textureType	= GL_TEXTURE_2D;
	opts.wrap			= GL_CLAMP_TO_BORDER;
	state.textureManager->Add( "Output Buffer", opts );

	// atomic tally images, for the renderer to write to
	opts.dataType		= GL_R32UI;
	opts.minFilter		= GL_NEAREST;
	opts.magFilter		= GL_NEAREST;
	state.textureManager->Add( "R Tally Image", opts );
	state.textureManager->Add( "G Tally Image", opts );
	state.textureManager->Add( "B Tally Image", opts );
	state.textureManager->Add( "Sample Count", opts );

	// Adam's buffers
	uint32_t adamBufferSize = nextPowerOfTwo( std::max( state.dimensions.x, state.dimensions.y ) );
	// cout << "adam says... " << adamBufferSize << endl;
	int w = adamBufferSize;
	int h = adamBufferSize;

	// creating a square image
	opts = textureOptions_t();
	opts.textureType = GL_TEXTURE_2D;
	opts.width = w;
	opts.height = h;
	opts.dataType = GL_RGBA32F; // type tbd... may need to be ints, for atomic operations? Not sure what that's going to look like, in practical terms
	state.textureManager->Add( "Adam", opts );

	Image_4U zeroesU( w, h );
	Image_4F zeroesF( w, h );

	// creating the mip chain
	int level = 0;
	while ( h >= 1 ) {
		h /= 2; w /= 2; level++;

		glBindTexture( GL_TEXTURE_2D, state.textureManager->Get( "Adam" ) );
		glTexImage2D( GL_TEXTURE_2D, level, GL_RGBA32F, w, h, 0, getFormat( GL_RGBA32F ), GL_FLOAT, ( void * ) zeroesF.GetImageDataBasePtr() );
	}

	// set the number of levels that are in the texture
	state.numLevels = level;
}

uvec2 GetNextOffset ( icarusState_t &state ) {
	uvec2 offset;
	switch ( state.offsetFeedMode ) {
		case UNIFORM: {
			// uniform random
			static rng offsetGen = rng( 0.0f, 1.0f );
			offset = uvec2( offsetGen() * state.dimensions.x, offsetGen() * state.dimensions.y );
			break;
		}

		case GAUSSIAN: {
			// center-biased random, creates a foveated look
				// probably easiest to rejection sample for off-screen offsets
			static rngN offsetGen = rngN( 0.5f, 0.15f );
			do {
				offset = uvec2( offsetGen() * state.dimensions.x, offsetGen() * state.dimensions.y );
			} while ( offset.x >= state.dimensions.x || offset.x < 0 || offset.y >= state.dimensions.y || offset.y < 0 );
			break;
		}

		case SHUFFLED: {
			// list that ensures you will touch every pixel before repeating
			static vector< uvec2 > offsets;
			// generate new list of offsets
			if ( offsets.size() == 0 || state.forceUpdate ) {
				offsets.clear();
				state.forceUpdate = false;

				for ( uint32_t x = 0; x < state.dimensions.x; x++ ) {
					for ( uint32_t y = 0; y < state.dimensions.y; y++ ) {
						offsets.push_back( uvec2( x, y ) );
					}
				}
				// shuffle it around a bit
				static auto rng = std::default_random_engine {};
				std::shuffle( std::begin( offsets ), std::end( offsets ), rng );
				std::shuffle( std::begin( offsets ), std::end( offsets ), rng );
				std::shuffle( std::begin( offsets ), std::end( offsets ), rng );
			}

			// pop one off the list, return that
			// offset = offsets[ offsets.size() - 1 ];
			// offsets.pop_back();

			// just go through the list over and over
			static size_t idx = 0;
			idx = ( idx + 1 ) % offsets.size();
			offset = offsets[ idx ];

			break;
		}

		default: {
			offset = uvec2( 0 );
			break;
		}
	}

	return offset;
}

void AdamUpdate ( icarusState_t &state ) {

	// do the initial averaging, into Adam mip 0
	GLuint shader = state.CopyShader;
	glUseProgram( shader );

	glUniform2i( glGetUniformLocation( shader, "dims" ), state.dimensions.x, state.dimensions.y );
	
	state.textureManager->BindImageForShader( "R Tally Image", "rTally", shader, 0 );
	state.textureManager->BindImageForShader( "G Tally Image", "gTally", shader, 1 );
	state.textureManager->BindImageForShader( "B Tally Image", "bTally", shader, 2 );
	state.textureManager->BindImageForShader( "Sample Count", "count", shader, 3 );
	state.textureManager->BindImageForShader( "Adam", "adam", shader, 4 );

	glDispatchCompute( ( state.dimensions.x + 15 ) / 16, ( state.dimensions.y + 15 ) / 16, 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

	// the propagate that data back up through the mips
	shader = state.AdamShader;
	glUseProgram( shader );

	// this seems wrong...
	int w = ( 1 << state.numLevels ) / 4;
	int h = ( 1 << state.numLevels ) / 4;

	for ( int n = 0; n < state.numLevels - 1; n++ ) { // for num mips minus 1

		glUniform2i( glGetUniformLocation( shader, "dims" ), 2 * w, 2 * h );

		// bind the appropriate levels for N and N+1 (starting with N=0... to N=...? )
		state.textureManager->BindImageForShader( "Adam", "adamN", shader, 0, n );
		state.textureManager->BindImageForShader( "Adam", "adamNPlusOne", shader, 1, n + 1 );

		// this seems to be the most effective groupsize, got it down from ~4.7ms to ~0.7ms
		glDispatchCompute( ( w + 15 ) / 16, ( h + 15 ) / 16, 1 ); w /= 2; h /= 2;
		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	}
}

void PostProcess ( icarusState_t &state ) {
	const GLuint shader = state.PrepShader;
	glUseProgram( shader );

	// tbd... prepping an output image from the Adam representation
		// currently, just a passthrough, but will do the tonemapping and etc

	// output buffer holding the prepped color
	state.textureManager->BindImageForShader( "Output Buffer", "outputBuffer", shader, 0 );

	// Adam buffers
	state.textureManager->BindTexForShader( "Adam", "adam", shader, 1 );

	glDispatchCompute( ( state.dimensions.x + 15 ) / 16, ( state.dimensions.y + 15 ) / 16, 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
}

void DrawViewer ( icarusState_t &state, viewerState_t &viewerState ) {
	const GLuint shader = state.DrawShader;
	glUseProgram( shader );

	// use this for some time varying seeding type thing, maybe
	glUniform1f( glGetUniformLocation( shader, "time" ), SDL_GetTicks() / 1600.0f );
	glUniform2f( glGetUniformLocation( shader, "offset" ), viewerState.offset.x, viewerState.offset.y );
	glUniform1f( glGetUniformLocation( shader, "scale" ), viewerState.scale );

	state.textureManager->BindImageForShader( "Accumulator", "accumulator", shader, 0 );
	state.textureManager->BindTexForShader( "Output Buffer", "outputBuffer", shader, 1 );

	glDispatchCompute( ( viewerState.viewerSize.x + 15 ) / 16, ( viewerState.viewerSize.y + 15 ) / 16, 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
}

void ClearAccumulators ( icarusState_t &state ) {
	const GLuint shader = state.ClearShader;
	glUseProgram( shader );

	state.textureManager->BindImageForShader( "R Tally Image", "rTally", shader, 0 );
	state.textureManager->BindImageForShader( "G Tally Image", "gTally", shader, 1 );
	state.textureManager->BindImageForShader( "B Tally Image", "bTally", shader, 2 );
	state.textureManager->BindImageForShader( "Sample Count", "count", shader, 3 );
	state.textureManager->BindTexForShader( "Adam", "adam", shader, 4 );

	glDispatchCompute( ( state.dimensions.x + 15 ) / 16, ( state.dimensions.y + 15 ) / 16, 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
}

void Screenshot ( icarusState_t &state ) {

	const bool fullDepth = false;
	const bool srgbConvert = true;
	const string label = "Output Buffer";

	const GLuint tex = state.textureManager->Get( label );
	uvec2 dims = state.textureManager->GetDimensions( label );
	std::vector< float > imageBytesToSave;
	imageBytesToSave.resize( dims.x * dims.y * sizeof( float ) * 4, 0 );
	glBindTexture( GL_TEXTURE_2D, tex );
	glGetTexImage( GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, &imageBytesToSave.data()[ 0 ] );
	Image_4F screenshot( dims.x, dims.y, &imageBytesToSave.data()[ 0 ] );
	if ( srgbConvert == true ) {
		screenshot.RGBtoSRGB();
	}
	screenshot.SaturateAlpha();
	const string filename = string( "Icarus-" ) + timeDateString() + string( fullDepth ? ".exr" : ".png" );
	screenshot.Save( filename, fullDepth ? Image_4F::backend::TINYEXR : Image_4F::backend::LODEPNG );
}

void SystemUpdate ( icarusState_t &state, inputHandler_t &input ) {
	if ( input.getState4( KEY_R ) == KEYSTATE_RISING ) {
		ClearAccumulators( state );
	}

	if ( input.getState4( KEY_T ) == KEYSTATE_RISING ) {
		Screenshot( state );
	}

	if ( input.getState4( KEY_Y ) == KEYSTATE_RISING ) {
		CompileShaders( state );
	}
}

void CameraUpdate ( icarusState_t &state, inputHandler_t &input ) {
	const bool shift = input.getState( KEY_LEFT_SHIFT ) || input.getState( KEY_RIGHT_SHIFT );
	const bool control = input.getState( KEY_LEFT_CTRL ) || input.getState( KEY_RIGHT_CTRL );

	const float scalar = shift ? 0.1f : ( control ? 0.0005f : 0.02f );

	if ( input.getState( KEY_W ) ) {
		glm::quat rot = glm::angleAxis( scalar, state.basisX ); // basisX is the axis, therefore remains untransformed
		state.basisY = ( rot * vec4( state.basisY, 0.0f ) ).xyz();
		state.basisZ = ( rot * vec4( state.basisZ, 0.0f ) ).xyz();
	}
	if ( input.getState( KEY_S ) ) {
		glm::quat rot = glm::angleAxis( -scalar, state.basisX );
		state.basisY = ( rot * vec4( state.basisY, 0.0f ) ).xyz();
		state.basisZ = ( rot * vec4( state.basisZ, 0.0f ) ).xyz();
	}
	if ( input.getState( KEY_A ) ) {
		glm::quat rot = glm::angleAxis( -scalar, state.basisY ); // same as above, but basisY is the axis
		state.basisX = ( rot * vec4( state.basisX, 0.0f ) ).xyz();
		state.basisZ = ( rot * vec4( state.basisZ, 0.0f ) ).xyz();
	}
	if ( input.getState( KEY_D ) ) {
		glm::quat rot = glm::angleAxis( scalar, state.basisY );
		state.basisX = ( rot * vec4( state.basisX, 0.0f ) ).xyz();
		state.basisZ = ( rot * vec4( state.basisZ, 0.0f ) ).xyz();
	}
	if ( input.getState( KEY_Q ) ) {
		glm::quat rot = glm::angleAxis( scalar, state.basisZ ); // and again for basisZ
		state.basisX = ( rot * vec4( state.basisX, 0.0f ) ).xyz();
		state.basisY = ( rot * vec4( state.basisY, 0.0f ) ).xyz();
	}
	if ( input.getState( KEY_E ) ) {
		glm::quat rot = glm::angleAxis( -scalar, state.basisZ );
		state.basisX = ( rot * vec4( state.basisX, 0.0f ) ).xyz();
		state.basisY = ( rot * vec4( state.basisY, 0.0f ) ).xyz();
	}
	if ( input.getState( KEY_UP ) )		state.viewerPosition += scalar * state.basisZ;
	if ( input.getState( KEY_DOWN ) )		state.viewerPosition -= scalar * state.basisZ;
	if ( input.getState( KEY_RIGHT ) )		state.viewerPosition += scalar * state.basisX;
	if ( input.getState( KEY_LEFT ) )		state.viewerPosition -= scalar * state.basisX;
	if ( input.getState( KEY_PAGEDOWN ) )	state.viewerPosition += scalar * state.basisY;
	if ( input.getState( KEY_PAGEUP ) )	state.viewerPosition -= scalar * state.basisY;
	if ( input.getState( KEY_EQUALS ) )	state.FoV = state.FoV - 0.1f * scalar; // zoom in
	if ( input.getState( KEY_MINUS ) )		state.FoV = state.FoV + 0.1f * scalar; // zoom out
}

void RayUpdate ( icarusState_t &state ) {
	{ // update the buffer containing the pixel offsets
		uvec2 offsets[ state.numRays ] = { uvec2( 0 ) };
		for ( uint i = 0; i < state.numRays; i++ ) {
			offsets[ i ] = GetNextOffset( state );
		}
		// send the data
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, state.offsetsSSBO );
		glBufferData( GL_SHADER_STORAGE_BUFFER, 2 * sizeof( GLuint ) * state.numRays, ( GLvoid * ) &offsets[ 0 ], GL_DYNAMIC_COPY );
	}

	{ // use the offsets to generate rays (first shader)
		const GLuint shader = state.RayGenerateShader;
		glUseProgram( shader );

		// need to update the stuff related to the rendering
		glUniform1f( glGetUniformLocation( shader, "FoV" ), state.FoV );
		glUniform3fv( glGetUniformLocation( shader, "basisX" ), 1, glm::value_ptr( state.basisX ) );
		glUniform3fv( glGetUniformLocation( shader, "basisY" ), 1, glm::value_ptr( state.basisY ) );
		glUniform3fv( glGetUniformLocation( shader, "basisZ" ), 1, glm::value_ptr( state.basisZ ) );
		glUniform3fv( glGetUniformLocation( shader, "viewerPosition" ), 1, glm::value_ptr( state.viewerPosition ) );
		glUniform2f( glGetUniformLocation( shader, "imageDimensions" ), state.dimensions.x, state.dimensions.y );

		glUniform1i( glGetUniformLocation( shader, "cameraMode" ), state.cameraMode );
		glUniform1f( glGetUniformLocation( shader, "uvScaleFactor" ), state.uvScaleFactor );
		glUniform1i( glGetUniformLocation( shader, "DoFBokehMode" ), state.DoFBokehMode );
		glUniform1f( glGetUniformLocation( shader, "DoFRadius" ), state.DoFRadius );
		glUniform1f( glGetUniformLocation( shader, "DoFFocusDistance" ), state.DoFFocusDistance );
		glUniform1f( glGetUniformLocation( shader, "chromabScaleFactor" ), state.chromabScaleFactor );

		// blue noise texture
		static rngi noiseOffsetGen( 0, 512 );
		glUniform2i( glGetUniformLocation( shader, "noiseOffset" ), noiseOffsetGen(), noiseOffsetGen() );
		state.textureManager->BindImageForShader( "Blue Noise", "blueNoise", shader, 0 );

		// fixed size, x times 256 = state.numRays
		glDispatchCompute( state.numRays / 256, 1, 1 );
		glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );
	}

	// looping for bounces
	for ( uint i = 0; i < state.maxBounces; i++ ) {

		{ // set initial state for all intersection structs
			const GLuint shader = state.RayClearShader;
			glUseProgram( shader );
			glDispatchCompute( state.numRays / 256, 1, 1 );
			glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );
		}

		if ( state.runSDF ) { // intersect those rays with SDF geo
			const GLuint shader = state.RayIntersectShader_SDF;
			glUseProgram( shader );
			glDispatchCompute( state.numRays / 256, 1, 1 );
		}

		if ( state.runTriangle ) { // intersect those rays with a triangle
			const GLuint shader = state.RayIntersectShader_Triangle;
			glUseProgram( shader );
			glDispatchCompute( state.numRays / 256, 1, 1 );
		}

		if ( state.runVolume ) { // intersect those rays with the volume
			const GLuint shader = state.RayIntersectShader_Volume;
			glUseProgram( shader );
			glDispatchCompute( state.numRays / 256, 1, 1 );
		}

		{ // do the shading on the intersection results (third shader)
			const GLuint shader = state.RayShadeShader;
			glUseProgram( shader );

			// this pass writes colors... I think it makes sense to also write depth, but only on the first bounce
			state.textureManager->BindImageForShader( "R Tally Image", "rTally", shader, 0 );
			state.textureManager->BindImageForShader( "G Tally Image", "gTally", shader, 1 );
			state.textureManager->BindImageForShader( "B Tally Image", "bTally", shader, 2 );
			state.textureManager->BindImageForShader( "Sample Count", "count", shader, 3 );

			glDispatchCompute( state.numRays / 256, 1, 1 );

			// make sure image writes complete
			glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
		}
	}
}


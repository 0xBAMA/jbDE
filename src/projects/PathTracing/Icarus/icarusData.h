struct icarusState_t {
	// SSBOs... details tbd
	GLuint rayBuffer;

	// access to the texture manager
	textureManager_t * textureManager;

	// shaders
	GLuint CopyShader;
	GLuint AdamShader;
	GLuint PrepShader;
	GLuint DrawShader;

	// dimensions and related info
	uvec2 dimensions = uvec2( 1920, 1080 );
	int numLevels;

	// how are we generating primary ray locations
	#define UNIFORM  0
	#define GAUSSIAN 1
	#define SHUFFLED 2
	int offsetFeedMode = UNIFORM;
	GLuint offsetsSSBO;

	// holding the rays... double buffering? tbd
	GLuint raySSBO;
};

// =============================================================================================================
// Initialization
void CompileShaders ( icarusState_t &state ) {
	const string basePath = string( "./src/projects/PathTracing/Icarus/shaders/" );

	state.CopyShader = computeShader( basePath + "copy.cs.glsl" ).shaderHandle;
	state.AdamShader = computeShader( basePath + "adam.cs.glsl" ).shaderHandle;
	state.PrepShader = computeShader( basePath + "prep.cs.glsl" ).shaderHandle;
	state.DrawShader = computeShader( basePath + "draw.cs.glsl" ).shaderHandle;
	// ...

}

void AllocateTextures ( icarusState_t &state ) {
	static bool firstTime = true;
	if ( !firstTime ) {
		// todo: delete the existing textures, first
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
	cout << "adam says... " << adamBufferSize << endl;
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
			static rngN offsetGen = rngN( 0.5f, 0.125f );
			do {
				offset = uvec2( offsetGen() * state.dimensions.x, offsetGen() * state.dimensions.y );
			} while ( offset.x >= state.dimensions.x || offset.x < 0 || offset.y >= state.dimensions.y || offset.y < 0 );
			break;
		}

		case SHUFFLED: {
			// list that ensures you will touch every pixel before repeating
			static vector< uvec2 > offsets;
			if ( offsets.size() == 0 ) {
				// generate new list of offsets
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
			offset = offsets[ offsets.size() - 1 ];
			offsets.pop_back();

			break;
		}

		default: {
			offset = uvec2( 0 );
			break;
		}
	}

	return offset;
}

void UpdateOffsetList ( icarusState_t &state ) {
	// get N offsets (number of pixels to update per frame)

	// and put this list of offsets into an SSBO

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

void AllocateBuffers ( icarusState_t &state ) {
	// allocate the ray buffer
	glGenBuffers( 1, &state.offsetsSSBO );
	glGenBuffers( 1, &state.raySSBO );
}

// =============================================================================================================
// Passing Uniforms
// ...
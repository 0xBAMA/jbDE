struct icarusState_t {
	// SSBOs... details tbd
	GLuint rayBuffer;

	// access to the texture manager
	textureManager_t * textureManager;

	// shaders
	GLuint AdamShader;
	GLuint PrepShader;
	GLuint DrawShader;

	// dimensions and related info
	uvec2 dimensions = uvec2( 1920, 1080 );
	int numLevels;
};

// =============================================================================================================
// Initialization
void CompileShaders ( icarusState_t &state ) {
	const string basePath = string( "./src/projects/PathTracing/Icarus/shaders/" );

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

	// creating an image to hold results
	textureOptions_t opts;
	opts.width			= state.dimensions.x;
	opts.height			= state.dimensions.y;
	opts.dataType		= GL_RGBA32F;
	opts.minFilter		= GL_LINEAR;
	opts.magFilter		= GL_LINEAR;
	opts.textureType	= GL_TEXTURE_2D;
	opts.wrap			= GL_CLAMP_TO_BORDER;
	state.textureManager->Add( "Output Buffer", opts );

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

	int level = 0;
	while ( h >= 1 ) {
		h /= 2; w /= 2; level++;

		glBindTexture( GL_TEXTURE_2D, state.textureManager->Get( "Adam" ) );
		glTexImage2D( GL_TEXTURE_2D, level, GL_RGBA32F, w, h, 0, getFormat( GL_RGBA32F ), GL_FLOAT, ( void * ) zeroesF.GetImageDataBasePtr() );
	}

	// set the number of levels that are in the texture
	state.numLevels = level;
}

void AdamUpdate ( icarusState_t &state ) {

	// first, we will have to populate color + counts from the atomic R, G, B, and Count tallies...
		// that can all go into one texture... so that might be a perf win, too

	const GLuint shader = state.AdamShader;
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

}

// =============================================================================================================
// Passing Uniforms
// ...
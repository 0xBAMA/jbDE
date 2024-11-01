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
	GLuint RayGenerateShader;
	GLuint RayIntersectShader;
	GLuint RayShadeShader;
	GLuint WipeShader;

	// dimensions and related info
	uvec2 dimensions = uvec2( 1920, 1080 );
	int numLevels;

	// how are we generating primary ray locations
	#define UNIFORM		0
	#define GAUSSIAN	1
	#define SHUFFLED	2

	int offsetFeedMode = SHUFFLED;
	GLuint offsetsSSBO;

	uint numRays = 2 << 16;

	// holding the rays
	GLuint raySSBO;

	// camera parameterization
	vec3 viewerPosition = vec3( 0.0f );
	vec3 basisX = vec3( 1.0f, 0.0f, 0.0f );
	vec3 basisY = vec3( 0.0f, 1.0f, 0.0f );
	vec3 basisZ = vec3( 0.0f, 0.0f, 1.0f );
	float FoV = 0.618f;
	int cameraType = 0;

	// DoF stuff
	bool DoF = true;
	float DoFDistance = 2.5f;
	float DoFRadius = 0.1f;
};

// =============================================================================================================
// Initialization
void CompileShaders ( icarusState_t &state ) {
	const string basePath = string( "./src/projects/PathTracing/Icarus/shaders/" );

	state.CopyShader 			= computeShader( basePath + "copy.cs.glsl" ).shaderHandle;
	state.AdamShader			= computeShader( basePath + "adam.cs.glsl" ).shaderHandle;
	state.PrepShader			= computeShader( basePath + "prep.cs.glsl" ).shaderHandle;
	state.DrawShader			= computeShader( basePath + "draw.cs.glsl" ).shaderHandle;
	state.ClearShader			= computeShader( basePath + "clear.cs.glsl" ).shaderHandle;

	// ray generation, intersection, and shading
	state.RayGenerateShader		= computeShader( basePath + "rayGenerate.cs.glsl" ).shaderHandle;
	state.RayIntersectShader	= computeShader( basePath + "rayIntersect.cs.glsl" ).shaderHandle;
	state.RayShadeShader		= computeShader( basePath + "rayShade.cs.glsl" ).shaderHandle;

}

void AllocateBuffers ( icarusState_t &state ) {
	// allocate the ray buffer
	glGenBuffers( 1, &state.offsetsSSBO );		// pixel offsets
	glGenBuffers( 1, &state.raySSBO );		// ray state front and back buffers

	// allocate space for ray offsets - 2x ints * state.numRays offsets per frame
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, state.offsetsSSBO );
	glBufferData( GL_SHADER_STORAGE_BUFFER, 2 * sizeof( GLuint ) * state.numRays, NULL, GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, state.offsetsSSBO );

	// allocate space for the ray state structs, state.numRays of them - this is going to change a lot, as I figure out what needs to happen
		// currently 108 bytes, see rayState.h.glsl
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, state.raySSBO );
	glBufferData( GL_SHADER_STORAGE_BUFFER, 128 * state.numRays, NULL, GL_DYNAMIC_COPY );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, state.raySSBO);
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
			static rngN offsetGen = rngN( 0.5f, 0.15f );
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

		// fixed size, x times 256 = state.numRays
		glDispatchCompute( state.numRays / 256, 1, 1 );
		glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );
	}

	// looping for bounces
	for ( uint i = 0; i < 16; i++ ) {

		{ // intersect those rays with the scene (second shader)
			const GLuint shader = state.RayIntersectShader;
			glUseProgram( shader );

			glDispatchCompute( state.numRays / 256, 1, 1 );
			glMemoryBarrier( GL_SHADER_STORAGE_BARRIER_BIT );
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

// =============================================================================================================
// Passing Uniforms
// ...
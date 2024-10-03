// =============================================================================================================
// Initialization
void CompileShaders ( unordered_map< string, GLuint > &shaders ) {
	shaders[ "Draw" ] = computeShader( "./src/projects/PathTracing/Icarus/shaders/draw.cs.glsl" ).shaderHandle;
	// ...
}

void AllocateTextures ( textureManager_t &textureManager, uvec2 bufferResolution ) {
	static bool firstTime = true;
	if ( !firstTime ) {
		// delete the existing textures, first
	}
	firstTime = false;

	textureOptions_t opts;

	// creating an image to hold results
	opts.width			= bufferResolution.x;
	opts.height			= bufferResolution.y;
	opts.dataType		= GL_RGBA32F;
	opts.minFilter		= GL_LINEAR;
	opts.magFilter		= GL_LINEAR;
	opts.textureType	= GL_TEXTURE_2D;
	opts.wrap			= GL_CLAMP_TO_BORDER;

	textureManager.Add( "Output Buffer", opts );

	// Adam's buffers
	uint32_t adamBufferSize = nextPowerOfTwo( std::max( bufferResolution.x, bufferResolution.y ) );
		// count
		// color
}

struct icarusBuffers_t {
	GLuint rayBuffer;
};

void AllocateBuffers ( icarusBuffers_t &icarusBuffers ) {
	// allocate the ray buffer
}

// =============================================================================================================
// Passing Uniforms
// ...
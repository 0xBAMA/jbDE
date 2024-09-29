// =============================================================================================================
// Initialization
void CompileShaders ( unordered_map< string, GLuint > &shaders ) {
	shaders[ "Draw" ] = computeShader( "./src/projects/PathTracing/Icarus/shaders/draw.cs.glsl" ).shaderHandle;
	// ...
}

void AllocateTextures ( textureManager_t textureManager ) {
	// todo
}

void AllocateBuffers () {
	// todo - will need to think about how to make these accessible from the rest of the program
}

// =============================================================================================================
// Passing Uniforms
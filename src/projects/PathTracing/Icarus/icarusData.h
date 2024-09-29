void CompileShaders ( unordered_map< string, GLuint > &shaders ) {
	shaders[ "Draw" ] = computeShader( "./src/projects/PathTracing/Icarus/shaders/draw.cs.glsl" ).shaderHandle;
	// ...
}

void AllocateTextures ( textureManager_t textureManager ) {
	// todo
}
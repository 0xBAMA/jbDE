struct line {
	vec4 p0, p1;	// width in .a?
	vec4 color;
};

struct LineSpamConfig_t {
	ivec2 dimensions = ivec2( 1920, 1080 );

	textureManager_t * textureManager;

	GLuint opaqueLineSSBO;
	GLuint transparentLineSSBO;

	GLuint opaquePreZShader;
	GLuint opaqueDrawShader;
	GLuint transparentDrawShader;
	GLuint compositeShader;

	void CreateTextures();
	void CompileShaders();
	void PrepLineBuffers();

	void OpaquePass();
	void TransparentPass();
	void CompositePass();

	bool dirty = true;
	vector< line > lines;
	vector< line > opaqueLines;
	vector< line > transparentLines;
	void AddLine( line l ) { dirty = true; lines.push_back( l ); }
};

void LineSpamConfig_t::CreateTextures() {
	textureOptions_t opts;
	opts.dataType		= GL_R32UI;
	opts.width			= dimensions.x;
	opts.height			= dimensions.y;
	opts.minFilter		= GL_NEAREST;
	opts.magFilter		= GL_NEAREST;
	opts.textureType	= GL_TEXTURE_2D;
	opts.wrap			= GL_CLAMP_TO_BORDER;

	// depth buffer, uint
	textureManager->Add( "Depth Buffer", opts );

	// id buffer, uint
	textureManager->Add( "ID Buffer", opts );

	// r, g, b, count tallies, uint
	textureManager->Add( "Red Tally Buffer", opts );
	textureManager->Add( "Green Tally Buffer", opts );
	textureManager->Add( "Blue Tally Buffer", opts );
	textureManager->Add( "Sample Tally Buffer", opts );

	// color buffer, RGB color
	opts.dataType		= GL_RGBA16F;
	opts.magFilter		= GL_LINEAR;
	textureManager->Add( "Composite Target", opts );
}

void LineSpamConfig_t::CompileShaders() {
	const string basePath = string( "./src/projects/LineSpam/shaders/" );
	opaquePreZShader		= computeShader( basePath + "opaquePreZ.cs.glsl" ).shaderHandle;
	opaqueDrawShader		= computeShader( basePath + "opaqueDraw.cs.glsl" ).shaderHandle;
	transparentDrawShader	= computeShader( basePath + "transparentDraw.cs.glsl" ).shaderHandle;
	compositeShader			= computeShader( basePath + "composite.cs.glsl" ).shaderHandle;
}

void LineSpamConfig_t::PrepLineBuffers() {
	// unset flag
	dirty = false;

	// sorting the list of lines into opaque and transparent
	opaqueLines.clear();
	opaqueLines.reserve( lines.size() );
	transparentLines.clear();
	transparentLines.reserve( lines.size() );
	for ( auto& l : lines ) {
		if ( l.color.a == 1.0f ) {
			opaqueLines.push_back( l );
		} else {
			transparentLines.push_back( l );
		}
	}

	// create the buffers

}

void LineSpamConfig_t::OpaquePass() {
	// opaque pre z draw, establish closest depths per pixel
	glUseProgram( opaquePreZShader );
	textureManager->BindImageForShader( "Depth Buffer", "depthBuffer", opaquePreZShader, 0 );
	const int workgroupsRoundedUp = ( opaqueLines.size() + 63 ) / 64;
	glDispatchCompute( 64, std::max( workgroupsRoundedUp / 64, 1 ), 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

	// opaque draw, depth equals, setting ID values for composite
	glUseProgram( opaqueDrawShader );
	textureManager->BindImageForShader( "Depth Buffer", "depthBuffer", opaqueDrawShader, 0 );
	textureManager->BindImageForShader( "ID Buffer", "idBuffer", opaqueDrawShader, 1 );
	glDispatchCompute( 64, std::max( workgroupsRoundedUp / 64, 1 ), 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
}

void LineSpamConfig_t::TransparentPass() {
	// transparent draw process
	glUseProgram( transparentDrawShader );
	textureManager->BindImageForShader( "Red Tally Buffer", "redTally", transparentDrawShader, 0 );
	textureManager->BindImageForShader( "Green Tally Buffer", "greenTally", transparentDrawShader, 1 );
	textureManager->BindImageForShader( "Blue Tally Buffer", "blueTally", transparentDrawShader, 2 );
	textureManager->BindImageForShader( "Sample Tally Buffer", "sampleTally", transparentDrawShader, 3 );
	textureManager->BindImageForShader( "Depth Buffer", "depthBuffer", transparentDrawShader, 4 );
	const int workgroupsRoundedUp = ( transparentLines.size() + 63 ) / 64;
	glDispatchCompute( 64, std::max( workgroupsRoundedUp / 64, 1 ), 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
}

void LineSpamConfig_t::CompositePass() {
	// this will also do the buffer clears, since it's the only consumer of the other buffer info
	glUseProgram( compositeShader );
	textureManager->BindImageForShader( "Red Tally Buffer", "redTally", compositeShader, 0 );
	textureManager->BindImageForShader( "Green Tally Buffer", "greenTally", compositeShader, 1 );
	textureManager->BindImageForShader( "Blue Tally Buffer", "blueTally", compositeShader, 2 );
	textureManager->BindImageForShader( "Sample Tally Buffer", "sampleTally", compositeShader, 3 );
	textureManager->BindImageForShader( "Depth Buffer", "depthBuffer", compositeShader, 4 );
	textureManager->BindImageForShader( "ID Buffer", "idBuffer", compositeShader, 5 );
	textureManager->BindImageForShader( "Composite Target", "compositedResult", compositeShader, 6 );
	glDispatchCompute( ( dimensions.x + 15 ) / 16, ( dimensions.y + 15 ) / 16, 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
}
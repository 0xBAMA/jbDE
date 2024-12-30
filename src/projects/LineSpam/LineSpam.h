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

	void ClearPass();
	void OpaquePass();
	void TransparentPass();
	void CompositePass();

	bool dirty = true;
	vector< line > lines;
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
	vector< line > opaqueLines;
	vector< line > transparentLines;

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
	// opaque draw, depth equals, setting ID values for composite

}

void LineSpamConfig_t::TransparentPass() {
	// transparent draw process

}

void LineSpamConfig_t::CompositePass() {
	// this will also do the buffer clears, since it's the only consumer of the other buffer info

}
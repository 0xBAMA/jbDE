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

	// depth buffer, uint
	// id buffer, uint
	// r, g, b, count tallies, uint

	// color buffer, LDR RGB

}

void LineSpamConfig_t::CompileShaders() {
	// line passes
}

void LineSpamConfig_t::PrepLineBuffers() {
	dirty = false;
	// other buffer setup stuff...
}

void LineSpamConfig_t::ClearPass() {}
void LineSpamConfig_t::OpaquePass() {}
void LineSpamConfig_t::TransparentPass() {}
void LineSpamConfig_t::CompositePass() {}
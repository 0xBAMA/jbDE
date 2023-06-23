/*==============================================================================
need to handle the binding of images to the binding points for each shader
	- this will grow to include both image and texture usage
		*** texture manager will need to change to make use of this stuff, too
==============================================================================*/
struct binding {
	binding( int bp, GLuint te, GLenum ty ) : bindPoint( bp ), texture( te ), type( ty ) {}
	int bindPoint;
	GLuint texture;
	GLenum type;
};

struct bindSet {
	bindSet(){}
	bindSet( std::vector<binding> bs ) : bindings( bs ) {}
	std::vector<binding> bindings;
	void apply () {
		for ( auto b : bindings ) {
			glBindImageTexture( b.bindPoint, b.texture, 0, GL_TRUE, 0, GL_READ_WRITE, b.type );
		}
	}
};

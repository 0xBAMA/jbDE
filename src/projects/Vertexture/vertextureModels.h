#include <vector>
#include <string>

struct vertextureConfig {
	int numGoodGuys = 10;
	int numBadGuys = 10;
	int numTrees = 5;
	int numBoxes = 5;
};

// scoring, kills, etc, append a string
std::vector< std::string > eventReports;

// remapping float range
inline float Remap ( float value, float from1, float to1, float from2, float to2 ) {
	return ( value - from1 ) / ( to1 - from1 ) * ( to2 - from2 ) + from2;
}

// // helper for API geometry init
// struct groundPt {
// 	groundPt( glm::vec3 p, glm::uvec3 sC ) : position( p ), sColor( sC ) {}
// 	glm::vec3 position;	// position for the graphics API
// 	glm::uvec3 sColor;	// interpolated selection color
// };


// subdivided quad, displaced in the vertex shader
struct GroundModel {
	GLuint vao, vbo;
	GLuint shader;

	GLuint heightmap;

	float screenAR = 16.0f / 9.0f;

	const float scale = 1.618f;
	int numPoints = 0;

	// void subdivide ( std::vector<groundPt> &world, std::vector<glm::vec3> points ) {
	void subdivide ( std::vector<vec3> &world, std::vector<glm::vec3> points ) {
		const float minDisplacement = 0.01f;
		if ( glm::distance( points[ 0 ], points[ 2 ] ) < minDisplacement ) {
			// corner-to-corner distance is small, time to write API geometry

			glm::uvec3 A = { uint32_t( Remap( points[ 0 ].x, -scale, scale, 0.0f, 255.0f ) ), uint32_t( Remap( points[ 0 ].y, -scale, scale, 0.0f, 255.0f ) ), 0 };
			glm::uvec3 B = { uint32_t( Remap( points[ 1 ].x, -scale, scale, 0.0f, 255.0f ) ), uint32_t( Remap( points[ 1 ].y, -scale, scale, 0.0f, 255.0f ) ), 0 };
			glm::uvec3 C = { uint32_t( Remap( points[ 2 ].x, -scale, scale, 0.0f, 255.0f ) ), uint32_t( Remap( points[ 2 ].y, -scale, scale, 0.0f, 255.0f ) ), 0 };
			glm::uvec3 D = { uint32_t( Remap( points[ 3 ].x, -scale, scale, 0.0f, 255.0f ) ), uint32_t( Remap( points[ 3 ].y, -scale, scale, 0.0f, 255.0f ) ), 0 };

		// TODO: REVISE BASED ON ORIENTATION CHANGE, IF NEEDED
			//	A ( 0 )	@=======@ B ( 1 )
			//			|      /|
			//			|     / |
			//			|    /  |
			//			|   /   |
			//			|  /    |
			//			| /     |
			//			|/      |
			//	C ( 2 )	@=======@ D ( 3 ) --> X

			// // triangle 1 ABC
			// world.push_back( { points[ 0 ], A } );
			// world.push_back( { points[ 1 ], B } );
			// world.push_back( { points[ 2 ], C } );
			// // triangle 2 BCD
			// world.push_back( { points[ 1 ], B } );
			// world.push_back( { points[ 2 ], C } );
			// world.push_back( { points[ 3 ], D } );

			// triangle 1 ABC
			world.push_back( points[ 0 ] );
			world.push_back( points[ 1 ] );
			world.push_back( points[ 2 ] );
			// triangle 2 BCD
			world.push_back( points[ 1 ] );
			world.push_back( points[ 2 ] );
			world.push_back( points[ 3 ] );
		} else {
			glm::vec3 center = ( points[ 0 ]+  points[ 1 ] + points[ 2 ] + points[ 3 ] ) / 4.0f;
			// midpoints between corners
			glm::vec3 midpoint13 = ( points[ 1 ] + points[ 3 ] ) / 2.0f;
			glm::vec3 midpoint01 = ( points[ 0 ] + points[ 1 ] ) / 2.0f;
			glm::vec3 midpoint23 = ( points[ 2 ] + points[ 3 ] ) / 2.0f;
			glm::vec3 midpoint02 = ( points[ 0 ] + points[ 2 ] ) / 2.0f;
			// recursive calls, next level of subdivision
			subdivide( world, { midpoint01, points[ 1 ], center, midpoint13 } );
			subdivide( world, { points[ 0 ], midpoint01, midpoint02, center } );
			subdivide( world, { center, midpoint13, midpoint23, points[ 3 ] } );
			subdivide( world, { midpoint02, center, points[ 2 ], midpoint23 } );
		}
	}

	GroundModel ( GLuint sIn ) : shader( sIn ) {
		// std::vector<groundPt> world;
		std::vector<glm::vec3> world;
		std::vector<glm::vec3> basePoints;

		basePoints.resize( 4 );

		basePoints[ 0 ] = glm::vec3( -scale, -scale, 0.0f );
		basePoints[ 1 ] = glm::vec3( -scale,  scale, 0.0f );
		basePoints[ 2 ] = glm::vec3(  scale, -scale, 0.0f );
		basePoints[ 3 ] = glm::vec3(  scale,  scale, 0.0f );

		cout << "Starting subdivision" << endl;
		subdivide( world, basePoints );

		cout << "created " << world.size() << " points" << newline << newline;

		glGenVertexArrays( 1, &vao );
		glBindVertexArray( vao );
		glGenBuffers( 1, &vbo );
		glBindBuffer( GL_ARRAY_BUFFER, vbo );
		numPoints = world.size();
		size_t numBytesPoints = sizeof( glm::vec3 ) * numPoints;
		glBufferData( GL_ARRAY_BUFFER, numBytesPoints, NULL, GL_STATIC_DRAW );
		glBufferSubData( GL_ARRAY_BUFFER, 0, numBytesPoints, &world[ 0 ] );

		GLuint vPosition = glGetAttribLocation( shader, "vPosition" );
		glEnableVertexAttribArray( vPosition );
		glVertexAttribPointer( vPosition, 3, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( 0 ) ) );

		Image_4U heightmapImage( "./src/projects/Vertexture/textures/rock_height.png" );
		glGenTextures( 1, &heightmap );
		glActiveTexture( GL_TEXTURE0 + 0 ); // Texture unit 0
		glBindTexture( GL_TEXTURE_2D, heightmap );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 2048, 2048, 0, GL_RGBA, GL_UNSIGNED_BYTE, heightmapImage.GetImageDataBasePtr() );
		glGenerateMipmap( GL_TEXTURE_2D );

	}

	void Update ( int t ) {

	}

	void Display ( bool selectMode = false ) {
		// glClearColor( 0, 0, 0, 0 );
		// glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

		float time = TotalTime() / 10000.0f;

		glBindVertexArray( vao );
		glUseProgram( shader );

		glEnable( GL_DEPTH_TEST );

		glActiveTexture( GL_TEXTURE0 + 0 ); // Texture unit 0
		glBindTexture( GL_TEXTURE_2D, heightmap );

		glUniform1f( glGetUniformLocation( shader, "time" ), time );
		glUniform1f( glGetUniformLocation( shader, "AR" ), screenAR );
		glUniform1i( glGetUniformLocation( shader, "heightmap" ), 0 );

		glDrawArrays( GL_TRIANGLES, 0, numPoints );
	}
};

// point rendering, for gameplay entities
struct DudesAndTreesModel {
	GLuint vao, vbo;

	DudesAndTreesModel () {

	}

	void Update ( int t ) {

	}

	void Display () {

	}
};

// sideskirts for the groundModel
struct SkirtModel {
	GLuint vao, vbo;

	SkirtModel () {

	}

	void Update ( int t ) {

	}

	void Display () {

	}
};

// textured, transparent quad to represent the water
struct WaterModel {
	GLuint vao, vbo;

	WaterModel () {

	}

	void Update ( int t ) {

	}

	void Display () {

	}
};
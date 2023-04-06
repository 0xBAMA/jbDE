#include <vector>
#include <string>
#include "../../../../src/engine/coreUtils/random.h"

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

constexpr float globalScale = 1.618f;

//=====================================================================================================================
//===== Ground ========================================================================================================
// subdivided quad, displaced in the vertex shader
struct GroundModel {
	GLuint vao, vbo;
	GLuint shader;

	GLuint heightmap;

	float screenAR;
	const float scale = globalScale;
	int numPoints = 0;

	// void subdivide ( std::vector<groundPt> &world, std::vector<glm::vec3> points ) {
	void subdivide ( std::vector<vec3> &world, std::vector<glm::vec3> points ) {
		const float minDisplacement = 0.01f;
		if ( glm::distance( points[ 0 ], points[ 2 ] ) < minDisplacement ) {
			// corner-to-corner distance is small, time to write API geometry

			// glm::vec3 A = { Remap( points[ 0 ].x, -scale, scale, 0.0f, 1.0f ), Remap( points[ 0 ].y, -scale, scale, 0.0f, 1.0f ), 0 };
			// glm::vec3 B = { Remap( points[ 1 ].x, -scale, scale, 0.0f, 1.0f ), Remap( points[ 1 ].y, -scale, scale, 0.0f, 1.0f ), 0 };
			// glm::vec3 C = { Remap( points[ 2 ].x, -scale, scale, 0.0f, 1.0f ), Remap( points[ 2 ].y, -scale, scale, 0.0f, 1.0f ), 0 };
			// glm::vec3 D = { Remap( points[ 3 ].x, -scale, scale, 0.0f, 1.0f ), Remap( points[ 3 ].y, -scale, scale, 0.0f, 1.0f ), 0 };

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
		subdivide( world, basePoints );

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
		glActiveTexture( GL_TEXTURE9 ); // Texture unit 9
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

	glm::mat3 tridentM;
	void Display ( bool selectMode = false ) {
		// glClearColor( 0, 0, 0, 0 );
		// glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

		glBindVertexArray( vao );
		glUseProgram( shader );

		glEnable( GL_DEPTH_TEST );

		glUniform1f( glGetUniformLocation( shader, "time" ), TotalTime() / 10000.0f );
		glUniform1f( glGetUniformLocation( shader, "AR" ), screenAR );
		glUniform1i( glGetUniformLocation( shader, "heightmap" ), 9 );
		glUniformMatrix3fv( glGetUniformLocation( shader, "trident" ),
			1, GL_FALSE, glm::value_ptr( tridentM ) );

		glDrawArrays( GL_TRIANGLES, 0, numPoints );
	}
};

//=====================================================================================================================
//===== Skirts ========================================================================================================
// sideskirts for the groundModel
struct SkirtsModel {
	GLuint vao, vbo;
	GLuint shader;

	float screenAR;
	const float scale = globalScale;
	int numPoints = 0;

	SkirtsModel () {

	}

	void Update ( int t ) {

	}

	glm::mat3 tridentM;
	void Display () {

	}
};

//=====================================================================================================================
//===== Sphere ========================================================================================================
// point rendering, for gameplay entities
struct SphereModel {
	GLuint vao, vbo;
	GLuint shader;
	GLuint sphereImage;

	float screenAR;
	const float scale = globalScale;
	int numStaticPoints = 0;


	SphereModel ( GLuint sIn ) : shader( sIn ) {

		rng gen( 0.3f, 1.2f );
		rng genP( 1.0f, 6.0f );
		rng genD( -scale, scale );
		rngi flip( -1, 1 );

		std::vector<glm::vec4> points;

		// for ( float y = -1.0f; y < 1.0f; y += 0.01618f ) {
		// 	for ( float x = -1.0f; x < 1.0f; x += 0.01618f ) {
		// 		// points.push_back( glm::vec4( x, y, 0.0f, gen() ) );
		// 		points.push_back( glm::vec4( genD(), genD(), 0.0f, genP() * gen() ) );
		// 	}
		// }

		for ( int i = 0; i < 30000; i++ ) {
			// points.push_back( glm::vec4( genD(), genD(), gen(), genP() * gen() ) );
			points.push_back( glm::vec4( genD(), genD(), 0.0f, genP() * gen() ) );
		}


		// rng genAngle( 0.0f, pi * 2.0f );
		// rng genDist( 0.0f, 1.0f );
		// rng genRad( 0.0f, 0.3f );
		// for ( int i = 0; i < 100000; i++ ) {
		// 	const float a = genAngle();
		// 	const float b = genAngle();
		// 	const float c = genDist();
		// 	const float r = genRad();

		// 	glm::vec3 loc = glm::vec3( sin( a ) * r, cos( a ) * r + 1.0f, 0.0f ) * c;
		// 	loc = glm::rotate( loc, b, glm::vec3( 0.0f, 0.0f, 1.0f ) );

		// 	points.push_back( glm::vec4( loc.x, loc.y, loc.z, genP() * gen() ) );
		// }

		// rng treeBasePtGen( -globalScale, globalScale );
		// rng diameterGen( 1.0f, 12.0f );

		// rng placementDiameter( 0.0f, 0.2f );
		// rng placementAngle( 0.0f, pi * 2.0f );

		// for ( int i = 0; i < 100; i++ ) {
		// 	glm::vec2 treeBasePt( treeBasePtGen(), treeBasePtGen() );
		// 	rng heightGen( 0.02f, 1.4f );

		// 	int d = int( diameterGen() );
		// 	for ( int j = 0; j < 20 * d; j++ ) {
		// 		const float h = heightGen();
		// 		const float a = placementAngle();
		// 		const float d = placementDiameter();
		// 		points.push_back( glm::vec4( cos( a ) * d, sin( a ) * d, h, diameterGen() * h * 3.0f ) + vec4( treeBasePt, 0.0f, 0.0f ) );
		// 	}
		// }

		cout << "points.size() is " << points.size() << newline;

		glGenVertexArrays( 1, &vao );
		glBindVertexArray( vao );
		glGenBuffers( 1, &vbo );
		glBindBuffer( GL_ARRAY_BUFFER, vbo );
		numStaticPoints = points.size();
		size_t numBytesPoints = sizeof( glm::vec4 ) * numStaticPoints;
		glBufferData( GL_ARRAY_BUFFER, numBytesPoints, NULL, GL_STATIC_DRAW );
		glBufferSubData( GL_ARRAY_BUFFER, 0, numBytesPoints, &points[ 0 ] );

		// some set of static points, loaded into the vbo - these won't change, they get generated once, they know where to read from
			// the texture for the height, and then they

		GLuint vPosition = glGetAttribLocation( shader, "vPosition" );
		glEnableVertexAttribArray( vPosition );
		glVertexAttribPointer( vPosition, 4, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( 0 ) ) );

		Image_4U heightmapImage( "./src/projects/Vertexture/textures/sphere.png" );
		glGenTextures( 1, &sphereImage );
		glActiveTexture( GL_TEXTURE10 ); // Texture unit 9
		glBindTexture( GL_TEXTURE_2D, sphereImage );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, heightmapImage.GetImageDataBasePtr() );
		glGenerateMipmap( GL_TEXTURE_2D );

	}

	void Update ( int t ) {

	}

	glm::mat3 tridentM;
	void Display () {
		glBindVertexArray( vao );
		glUseProgram( shader );

		glEnable( GL_DEPTH_TEST );
		glEnable( GL_PROGRAM_POINT_SIZE );

		glPointParameteri( GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT );

		glPointSize( 24.0f );

		glUniform1f( glGetUniformLocation( shader, "time" ), TotalTime() / 10000.0f );
		glUniform1f( glGetUniformLocation( shader, "AR" ), screenAR );
		glUniform1i( glGetUniformLocation( shader, "heightmap" ), 9 );
		glUniform1i( glGetUniformLocation( shader, "sphere" ), 10 );
		glUniformMatrix3fv( glGetUniformLocation( shader, "trident" ),
			1, GL_FALSE, glm::value_ptr( tridentM ) );

		glDrawArrays( GL_POINTS, 0, numStaticPoints );
	}
};

//=====================================================================================================================
//===== Water =========================================================================================================
// textured, transparent quad to represent the water
struct WaterModel {
	GLuint vao, vbo;
	GLuint shader;

	float screenAR;
	const float scale = globalScale;
	int numPoints = 0;

	GLuint waterColorTexture;
	GLuint waterNormalTexture;
	GLuint waterHeightTexture;

	void subdivide ( std::vector<vec3> &world, std::vector<glm::vec3> points ) {
		const float minDisplacement = 0.01f;
		if ( glm::distance( points[ 0 ], points[ 2 ] ) < minDisplacement ) {
			// corner-to-corner distance is small, time to write API geometry

			//	A ( 0 )	@=======@ B ( 1 )
			//			|      /|
			//			|     / |
			//			|    /  |
			//			|   /   |
			//			|  /    |
			//			| /     |
			//			|/      |
			//	C ( 2 )	@=======@ D ( 3 ) --> X

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

	WaterModel ( GLuint sIn ) : shader( sIn ) {
		std::vector<glm::vec3> world;
		std::vector<glm::vec3> basePoints;

		basePoints.resize( 4 );

		basePoints[ 0 ] = glm::vec3( -scale, -scale, 0.0f );
		basePoints[ 1 ] = glm::vec3( -scale,  scale, 0.0f );
		basePoints[ 2 ] = glm::vec3(  scale, -scale, 0.0f );
		basePoints[ 3 ] = glm::vec3(  scale,  scale, 0.0f );
		subdivide( world, basePoints );

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

		Image_4U color( "./src/projects/Vertexture/textures/water_color.png" );
		Image_4U normal( "./src/projects/Vertexture/textures/water_norm.png" );
		Image_4U height( "./src/projects/Vertexture/textures/water_height.png" );

		glGenTextures( 1, &waterColorTexture );
		glActiveTexture( GL_TEXTURE11 );
		glBindTexture( GL_TEXTURE_2D, waterColorTexture );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, color.Width(), color.Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, color.GetImageDataBasePtr() );
		glGenerateMipmap( GL_TEXTURE_2D );

		glGenTextures( 1, &waterNormalTexture );
		glActiveTexture( GL_TEXTURE12 );
		glBindTexture( GL_TEXTURE_2D, waterNormalTexture );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, normal.Width(), normal.Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, normal.GetImageDataBasePtr() );
		glGenerateMipmap( GL_TEXTURE_2D );

		glGenTextures( 1, &waterHeightTexture );
		glActiveTexture( GL_TEXTURE13 );
		glBindTexture( GL_TEXTURE_2D, waterHeightTexture );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, height.Width(), height.Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, height.GetImageDataBasePtr() );
		glGenerateMipmap( GL_TEXTURE_2D );

	}

	void Update ( int t ) {

	}

	glm::mat3 tridentM;
	void Display () {
		glBindVertexArray( vao );
		glUseProgram( shader );

		glEnable( GL_DEPTH_TEST );

		glUniform1f( glGetUniformLocation( shader, "time" ), TotalTime() / 10000.0f );
		glUniform1f( glGetUniformLocation( shader, "AR" ), screenAR );
		glUniform1i( glGetUniformLocation( shader, "colorMap" ), 11 );
		glUniform1i( glGetUniformLocation( shader, "normalMap" ), 12 );
		glUniform1i( glGetUniformLocation( shader, "heightMap" ), 13 );
		glUniformMatrix3fv( glGetUniformLocation( shader, "trident" ),
			1, GL_FALSE, glm::value_ptr( tridentM ) );

		glDrawArrays( GL_TRIANGLES, 0, numPoints );
	}
};
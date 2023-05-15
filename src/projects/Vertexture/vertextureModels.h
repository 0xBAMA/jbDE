#include <vector>
#include <string>
#include "../../../../src/engine/coreUtils/random.h"
#include "../../../../src/utils/trident/trident.h"
#include "../../../../src/data/colors.h"

struct vertextureConfig {
	int numGoodGuys = 10;
	int numBadGuys = 10;
	int numTrees = 5;
	int numBoxes = 5;
};

// scoring, kills, etc, append a string
std::vector< std::string > eventReports;

// remapping float range from1..to1 -> from2..to2
inline float Remap ( float value, float from1, float to1, float from2, float to2 ) {
	return ( value - from1 ) / ( to1 - from1 ) * ( to2 - from2 ) + from2;
}

constexpr float globalScale = 1.618f;

//=====================================================================================================================
//===== Ground ========================================================================================================
// subdivided quad, displaced in the vertex shader
struct GroundModel {
	GLuint vao, vbo;
	GLuint shader;
	GLuint heightmap;

	mat3 tridentM;
	mat3 tridentD;

	float timeVal = 0.0f;
	float scale;
	float screenAR;
	int numPoints = 0;
	int numLights = 0;

	vec3 groundColor;

	void subdivide ( std::vector< vec3 > &world, std::vector< vec3 > points ) {
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
			vec3 center = ( points[ 0 ] +  points[ 1 ] + points[ 2 ] + points[ 3 ] ) / 4.0f;
			// midpoints between corners
			vec3 midpoint13 = ( points[ 1 ] + points[ 3 ] ) / 2.0f;
			vec3 midpoint01 = ( points[ 0 ] + points[ 1 ] ) / 2.0f;
			vec3 midpoint23 = ( points[ 2 ] + points[ 3 ] ) / 2.0f;
			vec3 midpoint02 = ( points[ 0 ] + points[ 2 ] ) / 2.0f;
			// recursive calls, next level of subdivision
			subdivide( world, { midpoint01, points[ 1 ], center, midpoint13 } );
			subdivide( world, { points[ 0 ], midpoint01, midpoint02, center } );
			subdivide( world, { center, midpoint13, midpoint23, points[ 3 ] } );
			subdivide( world, { midpoint02, center, points[ 2 ], midpoint23 } );
		}
	}

	GroundModel ( GLuint sIn ) : shader( sIn ) {
		std::vector< vec3 > world;
		std::vector< vec3 > basePoints;

		basePoints.resize( 4 );

		basePoints[ 0 ] = vec3( -globalScale, -globalScale, 0.0f );
		basePoints[ 1 ] = vec3( -globalScale,  globalScale, 0.0f );
		basePoints[ 2 ] = vec3(  globalScale, -globalScale, 0.0f );
		basePoints[ 3 ] = vec3(  globalScale,  globalScale, 0.0f );
		subdivide( world, basePoints );

		glGenVertexArrays( 1, &vao );
		glBindVertexArray( vao );
		glGenBuffers( 1, &vbo );
		glBindBuffer( GL_ARRAY_BUFFER, vbo );
		numPoints = world.size();
		size_t numBytesPoints = sizeof( vec3 ) * numPoints;
		glBufferData( GL_ARRAY_BUFFER, numBytesPoints, NULL, GL_STATIC_DRAW );
		glBufferSubData( GL_ARRAY_BUFFER, 0, numBytesPoints, &world[ 0 ] );

		GLuint vPosition = glGetAttribLocation( shader, "vPosition" );
		glEnableVertexAttribArray( vPosition );
		glVertexAttribPointer( vPosition, 3, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( 0 ) ) );

		// consider swapping out for a generated heightmap? something with ~10s of erosion applied?
		Image_4U heightmapImage( "./src/projects/Vertexture/textures/rock_height.png" );
		glGenTextures( 1, &heightmap );
		glActiveTexture( GL_TEXTURE9 ); // Texture unit 9
		glBindTexture( GL_TEXTURE_2D, heightmap );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, heightmapImage.Width(), heightmapImage.Height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, heightmapImage.GetImageDataBasePtr() );
		glGenerateMipmap( GL_TEXTURE_2D );

		rng gen( 0.0f, 1.0f );
		groundColor = palette::paletteRef( gen() + 0.5f, palette::type::paletteIndexed_interpolated );
	}

	void ShadowDisplay () {
		glBindVertexArray( vao );
		glUseProgram( shader );

		glEnable( GL_DEPTH_TEST );

		glUniform3f( glGetUniformLocation( shader, "groundColor" ), groundColor.x, groundColor.y, groundColor.z );
		glUniform1f( glGetUniformLocation( shader, "time" ), timeVal / 10000.0f );
		glUniform1f( glGetUniformLocation( shader, "AR" ), screenAR );
		glUniform1f( glGetUniformLocation( shader, "scale" ), scale );
		glUniform1i( glGetUniformLocation( shader, "heightmap" ), 9 );
		glUniformMatrix3fv( glGetUniformLocation( shader, "trident" ),
			1, GL_FALSE, glm::value_ptr( tridentD ) );

		glDrawArrays( GL_TRIANGLES, 0, numPoints );
	}

	void Display ( bool selectMode = false ) {
		glBindVertexArray( vao );
		glUseProgram( shader );

		glEnable( GL_DEPTH_TEST );

		glUniform3f( glGetUniformLocation( shader, "groundColor" ), groundColor.x, groundColor.y, groundColor.z );
		glUniform1f( glGetUniformLocation( shader, "time" ), timeVal / 10000.0f );
		glUniform1i( glGetUniformLocation( shader, "lightCount" ), numLights );
		glUniform1f( glGetUniformLocation( shader, "AR" ), screenAR );
		glUniform1f( glGetUniformLocation( shader, "scale" ), scale );
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

	float timeVal = 0.0f;
	float scale;
	float screenAR;
	int numPoints = 0;
	int numLights = 0;

	vec3 groundColor;

	SkirtsModel ( GLuint sIn ) : shader( sIn ) {
		std::vector< vec3 > world;
		std::vector< vec3 > basePoints;

		basePoints.resize( 4 );
		basePoints[ 0 ] = vec3( -globalScale, -globalScale,  1.0f );
		basePoints[ 1 ] = vec3( -globalScale, -globalScale, -1.5f );
		basePoints[ 2 ] = vec3(  globalScale, -globalScale,  1.0f );
		basePoints[ 3 ] = vec3(  globalScale, -globalScale, -1.5f );

		// triangle 1 ABC
		world.push_back( basePoints[ 0 ] );
		world.push_back( basePoints[ 1 ] );
		world.push_back( basePoints[ 2 ] );
		// triangle 2 BCD
		world.push_back( basePoints[ 2 ] );
		world.push_back( basePoints[ 1 ] );
		world.push_back( basePoints[ 3 ] );

		basePoints[ 0 ] = vec3( -globalScale, -globalScale,  1.0f );
		basePoints[ 1 ] = vec3( -globalScale, -globalScale, -1.5f );
		basePoints[ 2 ] = vec3( -globalScale,  globalScale,  1.0f );
		basePoints[ 3 ] = vec3( -globalScale,  globalScale, -1.5f );

		world.push_back( basePoints[ 0 ] );
		world.push_back( basePoints[ 2 ] );
		world.push_back( basePoints[ 1 ] );
		world.push_back( basePoints[ 1 ] );
		world.push_back( basePoints[ 2 ] );
		world.push_back( basePoints[ 3 ] );

		basePoints[ 0 ] = vec3(  globalScale, -globalScale,  1.0f );
		basePoints[ 1 ] = vec3(  globalScale, -globalScale, -1.5f );
		basePoints[ 2 ] = vec3(  globalScale,  globalScale,  1.0f );
		basePoints[ 3 ] = vec3(  globalScale,  globalScale, -1.5f );

		world.push_back( basePoints[ 0 ] );
		world.push_back( basePoints[ 1 ] );
		world.push_back( basePoints[ 2 ] );
		world.push_back( basePoints[ 2 ] );
		world.push_back( basePoints[ 1 ] );
		world.push_back( basePoints[ 3 ] );

		basePoints[ 0 ] = vec3(  globalScale,  globalScale,  1.0f );
		basePoints[ 1 ] = vec3(  globalScale,  globalScale, -1.5f );
		basePoints[ 2 ] = vec3( -globalScale,  globalScale,  1.0f );
		basePoints[ 3 ] = vec3( -globalScale,  globalScale, -1.5f );

		world.push_back( basePoints[ 0 ] );
		world.push_back( basePoints[ 1 ] );
		world.push_back( basePoints[ 2 ] );
		world.push_back( basePoints[ 2 ] );
		world.push_back( basePoints[ 1 ] );
		world.push_back( basePoints[ 3 ] );

		glGenVertexArrays( 1, &vao );
		glBindVertexArray( vao );
		glGenBuffers( 1, &vbo );
		glBindBuffer( GL_ARRAY_BUFFER, vbo );
		numPoints = world.size();
		size_t numBytesPoints = sizeof( vec3 ) * numPoints;
		glBufferData( GL_ARRAY_BUFFER, numBytesPoints, NULL, GL_STATIC_DRAW );
		glBufferSubData( GL_ARRAY_BUFFER, 0, numBytesPoints, &world[ 0 ] );

		GLuint vPosition = glGetAttribLocation( shader, "vPosition" );
		glEnableVertexAttribArray( vPosition );
		glVertexAttribPointer( vPosition, 3, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( 0 ) ) );
	}

	void ShadowDisplay () {

	}

	mat3 tridentM;
	mat3 tridentD;
	void Display () {
		glBindVertexArray( vao );
		glUseProgram( shader );

		glEnable( GL_DEPTH_TEST );

		glUniform3f( glGetUniformLocation( shader, "groundColor" ), groundColor.x, groundColor.y, groundColor.z );
		// glUniform1i( glGetUniformLocation( shader, "lightCount" ), numLights );
		glUniform1f( glGetUniformLocation( shader, "time" ), timeVal / 10000.0f );
		glUniform1f( glGetUniformLocation( shader, "AR" ), screenAR );
		glUniform1f( glGetUniformLocation( shader, "scale" ), scale );
		glUniform1i( glGetUniformLocation( shader, "heightmap" ), 9 );
		glUniform1i( glGetUniformLocation( shader, "waterHeight" ), 13 );
		glUniformMatrix3fv( glGetUniformLocation( shader, "trident" ),
			1, GL_FALSE, glm::value_ptr( tridentM ) );

		glDrawArrays( GL_TRIANGLES, 0, numPoints );
	}
};


//=====================================================================================================================
//===== Lights ========================================================================================================
// roving lights, apply simple shading to [ground, spheres] for a couple lights
struct LightsModel {
	GLuint movementShader;
	GLuint ssbo; // this will definitely need the SSBO, because it is responsible for creating the SSBO
	const int numFloatsPerLight = 8;
	const int numLights = 8; // tbd - if we do a large number, might want to figure out some way to do some type of culling?
	// alternatively, move to deferred shading, but that's a whole can of worms

	GLuint heightmap;
	GLuint distanceDirectionMap;

	float timeVal = 0.0f;
	mat3 tridentM;

	LightsModel ( GLuint sIn ) :
		movementShader( sIn ) {

		// create the SSBO and bind in slot 4

		// need to figure out what the buffer needs to hold
			// position ( vec3 + some extra value... we'll find a use for it )
			// color ( vec3 + intensity scalar )

		std::vector< GLfloat > initialSSBOData;
		rng location( -1.0f, 1.0f );
		rng zDistrib( 0.1f, 0.3f );
		rng colorPick( 0.6f, 0.8f );

		for ( int x = 0; x < numLights; x++ ) {
			// distribute initial light points
			initialSSBOData.push_back( location() );
			initialSSBOData.push_back( location() );
			initialSSBOData.push_back( zDistrib() );
			initialSSBOData.push_back( 0.0f );

			vec3 col = palette::paletteRef( colorPick(), palette::type::paletteIndexed_interpolated );
			initialSSBOData.push_back( col.r );
			initialSSBOData.push_back( col.g );
			initialSSBOData.push_back( col.b );
			initialSSBOData.push_back( 1.0f );
		}

		glGenBuffers( 1, &ssbo );
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ssbo );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * numFloatsPerLight * numLights, ( GLvoid * ) &initialSSBOData[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 4, ssbo );

	}

	// we don't need a draw function

	void Update ( int counter ) {
		rngi gen( 0, 100000 );

		// any required uniform setup
			// maybe count? not sure what we need yet

		// dispatch compute shader to update SSBO

		// for the time being this can be a no-op, we can just use the original light positions

	}
};

//=====================================================================================================================
//===== Sphere ========================================================================================================
// point rendering, for gameplay entities
struct SphereModel {
	GLuint vao, vbo;
	GLuint ssbo;
	GLuint shader, moverShader, movementShader, mapUpdateShader;
	GLuint sphereImage;

	GLuint steepness;
	GLuint distanceDirection;

	float timeVal = 0.0f;
	float scale;
	float screenAR;
	int numStaticPoints = 0;
	int dynamicPointCount = 0;
	int numLights = 0;

	// sqrt of num sim movers
	const int simQ = 16 * 5;

	uint32_t numTrees;
	std::vector< vec3 > obstacles; // x,y location, then radius

	SphereModel ( GLuint sIn, GLuint sInMover, GLuint sInMove, uint32_t nTrees ) :
		shader( sIn ), moverShader( sInMover ), movementShader( sInMove ), numTrees( nTrees ) {

		rng gen( 0.3f, 1.2f );
		rng genH( 0.0f, 0.15f );
		rng genP( 1.0f, 4.0f );
		rng genD( -globalScale, globalScale );
		rngi flip( -1, 1 );

		std::vector< vec4 > points;
		std::vector< vec4 > colors;

		// ground cover
		for ( int i = 0; i < 5000; i++ ) {
			points.push_back( vec4( genD(), genD(), genH(), genP() ) );
			colors.push_back( vec4( palette::paletteRef( genH() * 3.0f, palette::type::paletteIndexed_interpolated ), 1.0f ) );
		}

		rngN trunkJitter( 0.0f, 0.009f );
		rng trunkSizes( 5.0f, 8.0f );
		rng basePtPlace( -globalScale * 0.75f, globalScale * 0.75f );
		rng leafSizes( 6.0f, 14.0f );
		rngN foliagePlace( 0.0f, 0.1618f );
		for ( unsigned int i = 0; i < numTrees; i++ ) {
			const vec2 basePtOrig = vec2( basePtPlace(), basePtPlace() );

			vec2 basePt = basePtOrig;
			float constrict = 1.618f;
			float scalar = gen();
			rngN heightGen( scalar, 0.025f );

			obstacles.push_back( vec3( basePt.x, basePt.y, 0.06f ) );
			for ( float t = 0; t < scalar; t += 0.002f ) {
				basePt.x += trunkJitter() * 0.5f;
				basePt.y += trunkJitter() * 0.5f;
				constrict *= 0.999f;
				points.push_back( vec4( constrict * trunkJitter() + basePt.x, constrict * trunkJitter() + basePt.y, t, constrict * trunkSizes() ) );
				colors.push_back( vec4( palette::paletteRef( genH(), palette::type::paletteIndexed_interpolated ), 1.0f ) );
			}
			for ( int j = 0; j < 1000; j++ ) {
				rngN foliagePlace( 0.0f, 0.1f * globalScale );
				rngN foliageHeightGen( scalar, 0.05f );
				points.push_back( vec4( basePt.x + foliagePlace(), basePt.y + foliagePlace(), foliageHeightGen(), leafSizes() ) );
				colors.push_back( vec4( palette::paletteRef( genH() + 0.3f, palette::type::paletteIndexed_interpolated ), 1.0f ) );
			}
		}

		uint32_t numRocks = 10;
		rngN rockGen( 0.0f, 0.06f );
		rngN rockHGen( 0.1f, 0.06f );
		rngN rockSize( 6.0f, 2.5f );
		for ( unsigned int i = 0; i < numRocks; i++ ) {
			vec2 basePt = vec2( basePtPlace(), basePtPlace() );
			obstacles.push_back( vec3( basePt.x, basePt.y, 0.13f ) );
			for ( int l = 0; l < 1000; l++ ) {
				points.push_back( vec4( basePt.x + rockGen(), basePt.y + rockGen(), rockHGen(), rockSize() ) );
				colors.push_back( vec4( palette::paletteRef( genH() + 0.2f, palette::type::paletteIndexed_interpolated ), 1.0f ) );
			}
		}

		glGenVertexArrays( 1, &vao );
		glBindVertexArray( vao );
		glGenBuffers( 1, &vbo );
		glBindBuffer( GL_ARRAY_BUFFER, vbo );
		numStaticPoints = points.size();
		size_t numBytesPoints = sizeof( vec4 ) * numStaticPoints;
		size_t numBytesColors = sizeof( vec4 ) * numStaticPoints;
		glBufferData( GL_ARRAY_BUFFER, numBytesPoints + numBytesColors, NULL, GL_STATIC_DRAW );
		glBufferSubData( GL_ARRAY_BUFFER, 0, numBytesPoints, &points[ 0 ] );
		glBufferSubData( GL_ARRAY_BUFFER, numBytesPoints, numBytesColors, &colors[ 0 ] );

		// some set of static points, loaded into the vbo - these won't change, they get generated once, they know where to read from
			// the texture for the height, and then they

		GLuint vPosition = glGetAttribLocation( shader, "vPosition" );
		glEnableVertexAttribArray( vPosition );
		glVertexAttribPointer( vPosition, 4, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( 0 ) ) );

		GLuint vColor = glGetAttribLocation( shader, "vColor" );
		glEnableVertexAttribArray( vColor );
		glVertexAttribPointer( vColor, 4, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( numBytesPoints ) ) );

		std::vector< vec4 > ssboPoints;
		rng size( 3.0f, 6.0f );
		rng phase( 0.0f, pi * 2.0f );
		for ( int x = 0; x < simQ; x++ ) {
			for ( int y = 0; y < simQ; y++ ) {
				ssboPoints.push_back( vec4( 2.0f * globalScale * ( ( x / float( simQ ) ) - 0.5f ), 2.0f * globalScale * ( ( y / float( simQ ) ) - 0.5f ), 0.6f * genH(), size() ) );
				ssboPoints.push_back( vec4( palette::paletteRef( genH() + 0.5f, palette::type::paletteIndexed_interpolated ), phase() ) );
				dynamicPointCount++;
			}
		}

		glGenBuffers( 1, &ssbo );
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ssbo );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * dynamicPointCount, (GLvoid*) &ssboPoints[ 0 ],  GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, ssbo );

		Image_4U heightmapImage( "./src/projects/Vertexture/textures/sphere.png" );
		glGenTextures( 1, &sphereImage );
		glActiveTexture( GL_TEXTURE10 ); // Texture unit 10
		glBindTexture( GL_TEXTURE_2D, sphereImage );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, heightmapImage.GetImageDataBasePtr() );
		glGenerateMipmap( GL_TEXTURE_2D );

	}

	void Update ( int counter ) {
		rngi gen( 0, 100000 );

		glUseProgram( mapUpdateShader );
		glBindImageTexture( 1, steepness, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F );
		glBindImageTexture( 2, distanceDirection, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F );
		glUniform1i( glGetUniformLocation( mapUpdateShader, "heightmap" ), 9 );
		glUniform1i( glGetUniformLocation( mapUpdateShader, "inSeed" ), gen() );
		glUniform1i( glGetUniformLocation( mapUpdateShader, "numObstacles" ), obstacles.size() );
		glUniform3fv( glGetUniformLocation( mapUpdateShader, "obstacles" ), obstacles.size(), glm::value_ptr( obstacles[ 0 ] ) );
		glDispatchCompute( 512 / 16, 512 / 16, 1 );

		glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

		glUseProgram( movementShader );
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ssbo );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, ssbo );
		glUniform1f( glGetUniformLocation( movementShader, "time" ), timeVal / 10000.0f );
		glUniform1i( glGetUniformLocation( movementShader, "inSeed" ), gen() );
		glUniform1i( glGetUniformLocation( movementShader, "dimension" ), simQ );
		glDispatchCompute( simQ / 16, simQ / 16, 1 ); // dispatch the compute shader to update ssbo
	}

	void ShadowDisplay () {

	}

	mat3 tridentM;
	mat3 tridentD;
	void Display () {
		glBindVertexArray( vao );
		glUseProgram( shader );

		glEnable( GL_DEPTH_TEST );
		glEnable( GL_PROGRAM_POINT_SIZE );

		glPointParameteri( GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT );

		// static points
		glUniform1f( glGetUniformLocation( shader, "time" ), timeVal / 10000.0f );
		glUniform1i( glGetUniformLocation( shader, "lightCount" ), numLights );
		glUniform1f( glGetUniformLocation( shader, "AR" ), screenAR );
		glUniform1f( glGetUniformLocation( shader, "scale" ), scale );
		glUniform1i( glGetUniformLocation( shader, "heightmap" ), 9 );
		glUniform1i( glGetUniformLocation( shader, "sphere" ), 10 );
		glUniformMatrix3fv( glGetUniformLocation( shader, "trident" ),
			1, GL_FALSE, glm::value_ptr( tridentM ) );

		glDrawArrays( GL_POINTS, 0, numStaticPoints );

		// dynamic points
		glUseProgram( moverShader );
		glBindImageTexture( 1, steepness, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F );
		glBindImageTexture( 2, distanceDirection, 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F );
		glUniform1f( glGetUniformLocation( moverShader, "time" ), timeVal / 10000.0f );
		glUniform1i( glGetUniformLocation( moverShader, "lightCount" ), numLights );
		glUniform1f( glGetUniformLocation( moverShader, "AR" ), screenAR );
		glUniform1f( glGetUniformLocation( moverShader, "scale" ), scale );
		glUniform1i( glGetUniformLocation( moverShader, "heightmap" ), 9 );
		glUniform1i( glGetUniformLocation( moverShader, "sphere" ), 10 );
		glUniformMatrix3fv( glGetUniformLocation( moverShader, "trident" ),
			1, GL_FALSE, glm::value_ptr( tridentM ) );

		glDrawArrays( GL_POINTS, 0, dynamicPointCount );
	}
};

//=====================================================================================================================
//===== Water =========================================================================================================
// textured, transparent quad to represent the water
struct WaterModel {
	GLuint vao, vbo;
	GLuint shader;

	float timeVal = 0.0f;
	float scale;
	float screenAR;
	int numPoints = 0;
	int numLights = 0;

	GLuint waterColorTexture;
	GLuint waterNormalTexture;
	GLuint waterHeightTexture;

	void subdivide ( std::vector< vec3 > &world, std::vector< vec3 > points ) {
		const float minDisplacement = 0.01f;
		// corner-to-corner distance is small, time to write API geometry
		if ( glm::distance( points[ 0 ], points[ 2 ] ) < minDisplacement ) {
			// triangle 1 ABC
			world.push_back( points[ 0 ] );
			world.push_back( points[ 1 ] );
			world.push_back( points[ 2 ] );
			// triangle 2 BCD
			world.push_back( points[ 1 ] );
			world.push_back( points[ 2 ] );
			world.push_back( points[ 3 ] );
		} else {
			vec3 center = ( points[ 0 ] +  points[ 1 ] + points[ 2 ] + points[ 3 ] ) / 4.0f;
			// midpoints between corners
			vec3 midpoint13 = ( points[ 1 ] + points[ 3 ] ) / 2.0f;
			vec3 midpoint01 = ( points[ 0 ] + points[ 1 ] ) / 2.0f;
			vec3 midpoint23 = ( points[ 2 ] + points[ 3 ] ) / 2.0f;
			vec3 midpoint02 = ( points[ 0 ] + points[ 2 ] ) / 2.0f;
			// recursive calls, next level of subdivision
			subdivide( world, { midpoint01, points[ 1 ], center, midpoint13 } );
			subdivide( world, { points[ 0 ], midpoint01, midpoint02, center } );
			subdivide( world, { center, midpoint13, midpoint23, points[ 3 ] } );
			subdivide( world, { midpoint02, center, points[ 2 ], midpoint23 } );
		}
	}

	WaterModel ( GLuint sIn ) : shader( sIn ) {
		std::vector< vec3 > world;
		std::vector< vec3 > basePoints;

		basePoints.resize( 4 );

		basePoints[ 0 ] = vec3( -globalScale, -globalScale, 0.0f );
		basePoints[ 1 ] = vec3( -globalScale,  globalScale, 0.0f );
		basePoints[ 2 ] = vec3(  globalScale, -globalScale, 0.0f );
		basePoints[ 3 ] = vec3(  globalScale,  globalScale, 0.0f );
		subdivide( world, basePoints );

		glGenVertexArrays( 1, &vao );
		glBindVertexArray( vao );
		glGenBuffers( 1, &vbo );
		glBindBuffer( GL_ARRAY_BUFFER, vbo );
		numPoints = world.size();
		size_t numBytesPoints = sizeof( vec3 ) * numPoints;
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

	void ShadowDisplay () {

	}

	mat3 tridentM;
	mat3 tridentD;
	void Display () {
		glBindVertexArray( vao );
		glUseProgram( shader );

		glEnable( GL_DEPTH_TEST );

		glUniform1f( glGetUniformLocation( shader, "time" ), timeVal / 10000.0f );
		// glUniform1i( glGetUniformLocation( shader, "lightCount" ), numLights );
		glUniform1f( glGetUniformLocation( shader, "AR" ), screenAR );
		glUniform1f( glGetUniformLocation( shader, "scale" ), scale );
		glUniform1i( glGetUniformLocation( shader, "colorMap" ), 11 );
		glUniform1i( glGetUniformLocation( shader, "normalMap" ), 12 );
		glUniform1i( glGetUniformLocation( shader, "heightMap" ), 13 );
		glUniformMatrix3fv( glGetUniformLocation( shader, "trident" ),
			1, GL_FALSE, glm::value_ptr( tridentM ) );

		glDrawArrays( GL_TRIANGLES, 0, numPoints );
	}
};
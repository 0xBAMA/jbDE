#include <vector>
#include <string>
#include "../../../../src/engine/coreUtils/random.h"
#include "../../../../src/utils/trident/trident.h"
#include "../../../../src/data/colors.h"

// program config
struct vertextureConfig {

	// scaling the size of the GPU resources
	int Guys = 100;
	int Trees = 10;
	int Rocks = 10;
	int GroundCover = 100;
	int Lights = 16;

	// timekeeping since last reset
	float timeVal = 0.0f;

	// visualization parameters
	float scale = 0.4f;
	float heightScale = 0.2f;
	bool showLightDebugLocations = false;
	vec3 groundColor = vec3( 0.0f );

	// TODO: add some more stuff on this, to parameterize further - point sizes etc
		// also load from json file, for ability to hot reload
		//  e.g. R to regenerate reads from config file on disk, with all current edits ( don't have to recompile )
			// also do the shaders etc, so we have hot update on that

	// static / dynamic point counts, updated and reported during init
	int numPointsGround = 0;
	int numPointsSkirts = 0;
	int numPointsSpheres = 0;
	int numPointsMovingSpheres = 0;
	int numPointsWater = 0;

	// default orientation
	vec3 basisX = vec3(  0.610246f,  0.454481f,  0.648863f );
	vec3 basisY = vec3(  0.791732f, -0.321969f, -0.519100f );
	vec3 basisZ = vec3( -0.027008f,  0.830518f, -0.556314f );

	// for computing screen AR, from config.json
	uint32_t width = 640;
	uint32_t height = 350;
	float screenAR = ( float ) width / ( float ) height;

};

// graphics api resources
struct openGLResources {

	std::unordered_map< string, GLuint > VAOs;
	std::unordered_map< string, GLuint > VBOs;
	std::unordered_map< string, GLuint > SSBOs;
	std::unordered_map< string, GLuint > textures;
	std::unordered_map< string, GLuint > shaders;

	// eventually, framebuffer stuff for deferred work
		// depth
		// normals
		// albedo
		// ...

};

// scoring, kills, etc, append a string
std::vector< std::string > eventReports;

// remapping float range from1..to1 -> from2..to2
inline float Remap ( float value, float from1, float to1, float from2, float to2 ) {
	return ( value - from1 ) / ( to1 - from1 ) * ( to2 - from2 ) + from2;
}

// quad subdivision ( recursive )
inline void subdivide (
	std::vector< vec3 > &pointsVector,
	const std::vector< vec3 > inputPoints,
	const float minDisplacement = 0.01f ) {

	if ( glm::distance( inputPoints[ 0 ], inputPoints[ 2 ] ) < minDisplacement ) {
		/* corner-to-corner distance is small, time to write API geometry
			A ( 0 )	@=======@ B ( 1 )
					|      /|
					|     / |
					|    /  |
					|   /   |
					|  /    |
					| /     |
					|/      |
			C ( 2 )	@=======@ D ( 3 ) --> X */
		// triangle 1 ABC
		pointsVector.push_back( inputPoints[ 0 ] );
		pointsVector.push_back( inputPoints[ 1 ] );
		pointsVector.push_back( inputPoints[ 2 ] );
		// triangle 2 BCD
		pointsVector.push_back( inputPoints[ 1 ] );
		pointsVector.push_back( inputPoints[ 2 ] );
		pointsVector.push_back( inputPoints[ 3 ] );
	} else {
		const vec3 center = ( inputPoints[ 0 ] +  inputPoints[ 1 ] + inputPoints[ 2 ] + inputPoints[ 3 ] ) / 4.0f;
		// midpoints between corners
		const vec3 midpoint13 = ( inputPoints[ 1 ] + inputPoints[ 3 ] ) / 2.0f;
		const vec3 midpoint01 = ( inputPoints[ 0 ] + inputPoints[ 1 ] ) / 2.0f;
		const vec3 midpoint23 = ( inputPoints[ 2 ] + inputPoints[ 3 ] ) / 2.0f;
		const vec3 midpoint02 = ( inputPoints[ 0 ] + inputPoints[ 2 ] ) / 2.0f;
		// recursive calls, next level of subdivision
		subdivide( pointsVector, { midpoint01, inputPoints[ 1 ], center, midpoint13 }, minDisplacement );
		subdivide( pointsVector, { inputPoints[ 0 ], midpoint01, midpoint02, center }, minDisplacement );
		subdivide( pointsVector, { center, midpoint13, midpoint23, inputPoints[ 3 ] }, minDisplacement );
		subdivide( pointsVector, { midpoint02, center, inputPoints[ 2 ], midpoint23 }, minDisplacement );
	}
}

/* ==========================================================================================================================
Consolidating the resources for the API geometry
========================================================================================================================== */

struct APIGeometryContainer {

	APIGeometryContainer () {}
	~APIGeometryContainer () {}

	// init
	void LoadConfig ();	// read the relevant section of config.json
	void Initialize ();	// create the shaders, buffers and stuff, random init for points etc
	void InitReport ();	// tell how many points are being used, etc
	void Reset () {
		LoadConfig();
		Initialize();
		InitReport();
	}

	// shutdown
	void Terminate ();	// delete resources

	// main loop functions
	void Shadow ();	// update the shadowmap(s)
	void Render ();	// update the Gbuffer
	void Update ();	// run the movement compute shaders

		// TODO: function to composite the deferred data + light calcs into the VSRA accumulator

	// application data + state
	vertextureConfig config;
	openGLResources resources;

};

void APIGeometryContainer::LoadConfig () {
	// load the config from disk
	json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();

	// pull from j["app"]["Vertexture"][...]
		// this informs the behavior of Initialize()

	// ...

	// update AR with current value of screen dims
	config.screenAR = ( float ) config.width / ( float ) config.height;

}

void APIGeometryContainer::Initialize () {

	// if this is the first time that this has run
		// probably something we can do if we check for this behavior, something - tbd

	// create all the graphics api resources

	const string basePath( "./src/projects/Vertexture/shaders/" );
	resources.shaders[ "Background" ] = computeShader( basePath + "background.cs.glsl" ).shaderHandle;
	resources.shaders[ "Ground" ] = regularShader( basePath + "ground.vs.glsl", basePath + "ground.fs.glsl" ).shaderHandle;
	resources.shaders[ "Sphere" ] = regularShader( basePath + "sphere.vs.glsl", basePath + "sphere.fs.glsl" ).shaderHandle;
	resources.shaders[ "Sphere Movement" ] = computeShader( basePath + "movingSphere.cs.glsl" ).shaderHandle;
	resources.shaders[ "Light Movement" ] = computeShader( basePath + "movingLight.cs.glsl" ).shaderHandle;
	resources.shaders[ "Sphere Map Update" ] = computeShader( basePath + "movingSphereMaps.cs.glsl" ).shaderHandle;
	resources.shaders[ "Moving Sphere" ] = regularShader( basePath + "movingSphere.vs.glsl", basePath + "movingSphere.fs.glsl" ).shaderHandle;
	resources.shaders[ "Water" ] = regularShader( basePath + "water.vs.glsl", basePath + "water.fs.glsl" ).shaderHandle;
	resources.shaders[ "Skirts" ] = regularShader( basePath + "skirts.vs.glsl", basePath + "skirts.fs.glsl" ).shaderHandle;

	GLuint steepness, distanceDirection;
	Image_4U steepnessTex( 512, 512 );

	glGenTextures( 1, &steepness );
	glActiveTexture( GL_TEXTURE14 );
	glBindTexture( GL_TEXTURE_2D, steepness );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, steepnessTex.GetImageDataBasePtr() );
	resources.textures[ "Steepness Map" ] = steepness;

	glGenTextures( 1, &distanceDirection );
	glActiveTexture( GL_TEXTURE15 );
	glBindTexture( GL_TEXTURE_2D, distanceDirection );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, steepnessTex.GetImageDataBasePtr() );
	resources.textures[ "Distance/Direction Map" ] = distanceDirection;

	// pick a first palette ( lights )
	palette::PickRandomPalette();

		// randomly positioned + colored lights
			// ssbo, shader for movement

	// pick a second palette ( ground, skirts, spheres )
	palette::PickRandomPalette();

	// ground
		// shader, vao / vbo
		// heightmap texture
	{
		std::vector< vec3 > world;
		std::vector< vec3 > basePoints;

		basePoints.resize( 4 );
		basePoints[ 0 ] = vec3( -1.0f, -1.0f, 0.0f );
		basePoints[ 1 ] = vec3( -1.0f,  1.0f, 0.0f );
		basePoints[ 2 ] = vec3(  1.0f, -1.0f, 0.0f );
		basePoints[ 3 ] = vec3(  1.0f,  1.0f, 0.0f );
		subdivide( world, basePoints );

		GLuint vao, vbo, heightmap;
		glGenVertexArrays( 1, &vao );
		glBindVertexArray( vao );
		glGenBuffers( 1, &vbo );
		glBindBuffer( GL_ARRAY_BUFFER, vbo );
		config.numPointsGround = world.size();
		size_t numBytesPoints = sizeof( vec3 ) * config.numPointsGround;
		glBufferData( GL_ARRAY_BUFFER, numBytesPoints, NULL, GL_STATIC_DRAW );
		glBufferSubData( GL_ARRAY_BUFFER, 0, numBytesPoints, &world[ 0 ] );

		GLuint vPosition = glGetAttribLocation( resources.shaders[ "Ground" ], "vPosition" );
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

		resources.VAOs[ "Ground" ] = vao;
		resources.VBOs[ "Ground" ] = vbo;
		resources.textures[ "Heightmap" ] = heightmap;

		rng gen( 0.0f, 1.0f ); // this can probably be tuned better
		config.groundColor = palette::paletteRef( gen() + 0.5f, palette::type::paletteIndexed_interpolated );
	}

	// skirts
		// shader, vao / vbo
	{
		std::vector< vec3 > world;
		std::vector< vec3 > basePoints;

		basePoints.resize( 4 );
		basePoints[ 0 ] = vec3( -1.0, -1.0,  0.2f );
		basePoints[ 1 ] = vec3( -1.0, -1.0, -0.5f );
		basePoints[ 2 ] = vec3(  1.0, -1.0,  0.2f );
		basePoints[ 3 ] = vec3(  1.0, -1.0, -0.5f );

		// triangle 1 ABC
		world.push_back( basePoints[ 0 ] );
		world.push_back( basePoints[ 1 ] );
		world.push_back( basePoints[ 2 ] );
		// triangle 2 BCD
		world.push_back( basePoints[ 2 ] );
		world.push_back( basePoints[ 1 ] );
		world.push_back( basePoints[ 3 ] );

		basePoints[ 0 ] = vec3( -1.0, -1.0,  0.2f );
		basePoints[ 1 ] = vec3( -1.0, -1.0, -0.5f );
		basePoints[ 2 ] = vec3( -1.0,  1.0,  0.2f );
		basePoints[ 3 ] = vec3( -1.0,  1.0, -0.5f );

		world.push_back( basePoints[ 0 ] );
		world.push_back( basePoints[ 2 ] );
		world.push_back( basePoints[ 1 ] );
		world.push_back( basePoints[ 1 ] );
		world.push_back( basePoints[ 2 ] );
		world.push_back( basePoints[ 3 ] );

		basePoints[ 0 ] = vec3(  1.0, -1.0,  0.2f );
		basePoints[ 1 ] = vec3(  1.0, -1.0, -0.5f );
		basePoints[ 2 ] = vec3(  1.0,  1.0,  0.2f );
		basePoints[ 3 ] = vec3(  1.0,  1.0, -0.5f );

		world.push_back( basePoints[ 0 ] );
		world.push_back( basePoints[ 1 ] );
		world.push_back( basePoints[ 2 ] );
		world.push_back( basePoints[ 2 ] );
		world.push_back( basePoints[ 1 ] );
		world.push_back( basePoints[ 3 ] );

		basePoints[ 0 ] = vec3(  1.0,  1.0,  0.2f );
		basePoints[ 1 ] = vec3(  1.0,  1.0, -0.5f );
		basePoints[ 2 ] = vec3( -1.0,  1.0,  0.2f );
		basePoints[ 3 ] = vec3( -1.0,  1.0, -0.5f );

		world.push_back( basePoints[ 0 ] );
		world.push_back( basePoints[ 1 ] );
		world.push_back( basePoints[ 2 ] );
		world.push_back( basePoints[ 2 ] );
		world.push_back( basePoints[ 1 ] );
		world.push_back( basePoints[ 3 ] );

		GLuint vao, vbo;
		glGenVertexArrays( 1, &vao );
		glBindVertexArray( vao );
		glGenBuffers( 1, &vbo );
		glBindBuffer( GL_ARRAY_BUFFER, vbo );
		config.numPointsSkirts = world.size();
		size_t numBytesPoints = sizeof( vec3 ) * config.numPointsSkirts;
		glBufferData( GL_ARRAY_BUFFER, numBytesPoints, NULL, GL_STATIC_DRAW );
		glBufferSubData( GL_ARRAY_BUFFER, 0, numBytesPoints, &world[ 0 ] );

		resources.VAOs[ "Skirts" ] = vao;
		resources.VBOs[ "Skirts" ] = vbo;

		GLuint vPosition = glGetAttribLocation( resources.shaders[ "Skirts" ], "vPosition" );
		glEnableVertexAttribArray( vPosition );
		glVertexAttribPointer( vPosition, 3, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( 0 ) ) );
	}

	// spheres
		// sphere heightmap texture
			// try the math version, see if that's faster? from the nvidia paper
		// shader + vertex buffer of the static points
			// optionally include the debug spheres for the light positions
		// shader + ssbo buffer of the dynamic points
	{

	}

	// water
		// shader, vao / vbo
		// 3x textures
	{
		std::vector< vec3 > world;
		std::vector< vec3 > basePoints;

		basePoints.resize( 4 );

		basePoints[ 0 ] = vec3( -1.0f, -1.0f, 0.01f );
		basePoints[ 1 ] = vec3( -1.0f,  1.0f, 0.01f );
		basePoints[ 2 ] = vec3(  1.0f, -1.0f, 0.01f );
		basePoints[ 3 ] = vec3(  1.0f,  1.0f, 0.01f );
		subdivide( world, basePoints );

		GLuint vao, vbo;
		glGenVertexArrays( 1, &vao );
		glBindVertexArray( vao );
		glGenBuffers( 1, &vbo );
		glBindBuffer( GL_ARRAY_BUFFER, vbo );
		config.numPointsWater = world.size();
		size_t numBytesPoints = sizeof( vec3 ) * config.numPointsWater;
		glBufferData( GL_ARRAY_BUFFER, numBytesPoints, NULL, GL_STATIC_DRAW );
		glBufferSubData( GL_ARRAY_BUFFER, 0, numBytesPoints, &world[ 0 ] );

		GLuint vPosition = glGetAttribLocation( resources.shaders[ "Water" ], "vPosition" );
		glEnableVertexAttribArray( vPosition );
		glVertexAttribPointer( vPosition, 3, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( 0 ) ) );

		Image_4U color( "./src/projects/Vertexture/textures/water_color.png" );
		Image_4U normal( "./src/projects/Vertexture/textures/water_norm.png" );
		Image_4U height( "./src/projects/Vertexture/textures/water_height.png" );

		GLuint waterColorTexture;
		GLuint waterNormalTexture;
		GLuint waterHeightTexture;

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

		resources.textures[ "Water Color" ] = waterColorTexture;
		resources.textures[ "Water Normal" ] = waterNormalTexture;
		resources.textures[ "Water Height" ] = waterHeightTexture;
		resources.VAOs[ "Water" ] = vao;
		resources.VBOs[ "Water" ] = vbo;
	}

}

void APIGeometryContainer::InitReport () {

}

void APIGeometryContainer::Terminate () {

	// TODO: destroy the graphics api resources

}

void APIGeometryContainer::Shadow () {
	// =================================================================================================

	// shadowmapping resources -> will not be here
	// orientTrident tridentDepth;
	// GLuint shadowmapFramebuffer = 0;
	// GLuint depthTexture;

	// shadowmapping resources:
	//	http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-16-shadow-mapping/#rendering-the-shadow-map
	//	https://ogldev.org/www/tutorial23/tutorial23.html

	// create the shadowmap resources

	/*
	GLuint shadowmapFramebuffer = 0;
	glGenFramebuffers( 1, &shadowmapFramebuffer );
	glBindFramebuffer( GL_FRAMEBUFFER, shadowmapFramebuffer );

	// Depth texture - slower than a depth buffer, but you can sample it later in your shader
	glGenTextures( 1, &depthTexture );
	glBindTexture( GL_TEXTURE_2D, depthTexture );
	glTexImage2D( GL_TEXTURE_2D, 0,GL_DEPTH_COMPONENT16, 1024, 1024, 0,GL_DEPTH_COMPONENT, GL_FLOAT, 0 );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture, 0 );
	glDrawBuffer( GL_NONE ); // No color buffer is drawn to.

	if( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
		cout << "framebuffer creation failed" << endl; abort();
	}

	// revert to default framebuffer
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

		// I think eventually it's going to make more sense to just rewrite things to manage both
		// color and depth targets, so that we have more explicit state management. I'm not sure
		// what was causing the state leakage issue I was seeing but I have other things I want to
		// mess with instead.

		// And maybe that's something I'll want to carry forwards in the future - there is a number
		// of cool aspects there, you know, you can access that color data and do whatever postprocessing
		// that you want - also opens up deferred shading, which may be significant for this case
		// where you are writing depth for these primitives - keeping normals and depth to reconstruct
		// the world position, we can do whatever lighting on that flat target, and avoid having to
		// potentially shade the occluded fragments.. I'm not clear on if the current impl will have to


		// // prepare to render the shadowmap depth
		// glBindFramebuffer( GL_FRAMEBUFFER, shadowmapFramebuffer );
		// glClear( GL_DEPTH_BUFFER_BIT );

		// revert to default framebuffer
		// glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		// glClear( GL_DEPTH_BUFFER_BIT );


	*/

	// =================================================================================================
}

void APIGeometryContainer::Render () {

	// matrix for the view transform
	const mat3 tridentMat = mat3( config.basisX, config.basisY, config.basisZ );

	// ground
	glBindVertexArray( resources.VAOs[ "Ground" ] );
	glUseProgram( resources.shaders[ "Ground" ] );
	glEnable( GL_DEPTH_TEST );
	glUniform3f( glGetUniformLocation( resources.shaders[ "Ground" ], "groundColor" ), config.groundColor.x, config.groundColor.y, config.groundColor.z );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Ground" ], "time" ), config.timeVal / 10000.0f );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Ground" ], "heightScale" ), config.heightScale );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Ground" ], "lightCount" ), config.Lights );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Ground" ], "AR" ), config.screenAR );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Ground" ], "scale" ), config.scale );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Ground" ], "heightmap" ), 9 );
	glUniformMatrix3fv( glGetUniformLocation( resources.shaders[ "Ground" ], "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );
	glDrawArrays( GL_TRIANGLES, 0, config.numPointsGround );

	// spheres

	// water
	glBindVertexArray( resources.VAOs[ "Water" ] );
	glUseProgram( resources.shaders[ "Water" ] );
	glEnable( GL_DEPTH_TEST );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Water" ], "time" ), config.timeVal / 10000.0f );
	// glUniform1i( glGetUniformLocation( resources.shaders[ "Water" ], "lightCount" ), numLights );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Water" ], "heightScale" ), config.heightScale );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Water" ], "AR" ), config.screenAR );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Water" ], "scale" ), config.scale );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Water" ], "colorMap" ), 11 );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Water" ], "normalMap" ), 12 );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Water" ], "heightMap" ), 13 );
	glUniformMatrix3fv( glGetUniformLocation( resources.shaders[ "Water" ], "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );
	glDrawArrays( GL_TRIANGLES, 0, config.numPointsWater );

	// skirts
	glBindVertexArray( resources.VAOs[ "Skirts" ] );
	glUseProgram( resources.shaders[ "Skirts" ] );
	glEnable( GL_DEPTH_TEST );
	glUniform3f( glGetUniformLocation( resources.shaders[ "Skirts" ], "groundColor" ), config.groundColor.x, config.groundColor.y, config.groundColor.z );
	// glUniform1i( glGetUniformLocation( resources.shaders[ "Skirts" ], "lightCount" ), numLights );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Skirts" ], "heightScale" ), config.heightScale );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Skirts" ], "time" ), config.timeVal / 10000.0f );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Skirts" ], "AR" ), config.screenAR );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Skirts" ], "scale" ), config.scale );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Skirts" ], "heightmap" ), 9 );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Skirts" ], "waterHeight" ), 13 );
	glUniformMatrix3fv( glGetUniformLocation( resources.shaders[ "Skirts" ], "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );
	glDrawArrays( GL_TRIANGLES, 0, config.numPointsSkirts );

}

void APIGeometryContainer::Update () {

	// run the stuff to update the moving point locations
	// run the stuff to update the light positions

}
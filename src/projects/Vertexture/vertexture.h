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
	int Lights = 64;

	// timekeeping since last reset
	float timeVal = 0.0f;

	// visualization parameters
	float scale = 0.4f;
	float heightScale = 0.2f;
	bool showLightDebugLocations = false;
	vec3 groundColor = vec3( 0.0f );

	// output settings
	bool showTiming = false;
	bool showTrident = false;

	std::vector< vec3 > obstacles; // x,y location, then radius

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
	void Update ();	// run the movement compute shaders
	void Shadow ();	// update the shadowmap(s)
	void Render ();	// update the Gbuffer

		// TODO: function to composite the deferred data + light calcs into the VSRA accumulator

	// ImGui manipulation of program state
	void ControlWindow ();

	// application data + state
	vertextureConfig config;
	openGLResources resources;

};

void APIGeometryContainer::LoadConfig () {
	// load the config from disk
	json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();

	// this informs the behavior of Initialize()
	config.Guys			= j[ "app" ][ "Vertexture" ][ "Guys" ];
	config.Trees		= j[ "app" ][ "Vertexture" ][ "Trees" ];
	config.Rocks		= j[ "app" ][ "Vertexture" ][ "Rocks" ];
	config.Lights		= j[ "app" ][ "Vertexture" ][ "Lights" ];
	config.GroundCover	= j[ "app" ][ "Vertexture" ][ "GroundCover" ];
	config.showLightDebugLocations = j[ "app" ][ "Vertexture" ][ "ShowLightDebugLocations" ];

		// todo: write out the rest of the parameterization ( point sizes, counts, other distributions... )

	// other program state
	config.showTrident	= j[ "app" ][ "Vertexture" ][ "showTrident" ];
	config.showTiming	= j[ "app" ][ "Vertexture" ][ "showTiming" ];
	config.scale		= j[ "app" ][ "Vertexture" ][ "InitialScale" ];
	config.heightScale	= j[ "app" ][ "Vertexture" ][ "InitialHeightScale" ];
	config.basisX		= vec3( j[ "app" ][ "Vertexture" ][ "basisX" ][ "x" ], j[ "app" ][ "Vertexture" ][ "basisX" ][ "y" ], j[ "app" ][ "Vertexture" ][ "basisX" ][ "z" ] );
	config.basisY		= vec3( j[ "app" ][ "Vertexture" ][ "basisY" ][ "x" ], j[ "app" ][ "Vertexture" ][ "basisY" ][ "y" ], j[ "app" ][ "Vertexture" ][ "basisY" ][ "z" ] );
	config.basisZ		= vec3( j[ "app" ][ "Vertexture" ][ "basisZ" ][ "x" ], j[ "app" ][ "Vertexture" ][ "basisZ" ][ "y" ], j[ "app" ][ "Vertexture" ][ "basisZ" ][ "z" ] );

	// update AR with current value of screen dims
	config.width = j[ "screenWidth" ];
	config.height = j[ "screenHeight" ];
	config.screenAR = ( float ) config.width / ( float ) config.height;
}

void APIGeometryContainer::Initialize () {

	// if this is the first time that this has run
		// probably something we can do if we check for this behavior, something - tbd

	// clear existing list of obstacles
	config.obstacles.resize( 0 );

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
	std::vector< GLfloat > lightData;
	{
		GLuint ssbo;
		rng location( -1.0f, 1.0f );
		rng zDistrib( 0.2f, 0.6f );
		rng colorPick( 0.6f, 0.8f );
		rng brightness( 0.01f, 0.026f );

		for ( int x = 0; x < config.Lights; x++ ) {
		// need to figure out what the buffer needs to hold
			// position ( vec3 + some extra value... we'll find a use for it )
			// color ( vec3 + some extra value, again we'll find some kind of use for it )

			// distribute initial light points
			lightData.push_back( location() );
			lightData.push_back( location() );
			lightData.push_back( zDistrib() );
			lightData.push_back( 0.0f );

			vec3 col = palette::paletteRef( colorPick(), palette::type::paletteIndexed_interpolated ) * brightness();
			lightData.push_back( col.r );
			lightData.push_back( col.g );
			lightData.push_back( col.b );
			lightData.push_back( 1.0f );
		}

		// create the SSBO and bind in slot 4
		glGenBuffers( 1, &ssbo );
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ssbo );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * config.Lights, ( GLvoid * ) &lightData[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 4, ssbo );

		resources.SSBOs[ "Lights" ] = ssbo;
	}

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
		std::vector< vec4 > points;
		std::vector< vec4 > colors;

		// these should come from the config
		rng gen( 0.185f, 0.74f );
		rng genH( 0.0f, 0.04f );
		rng genP( 1.85f, 3.7f );
		rng genD( -1.0f, 1.0f );
		rngi flip( -1, 1 );
		rng gen_normalize( 0.01f, 40.0f );
		for ( int i = 0; i < config.GroundCover; i++ ) { // ground cover
			points.push_back( vec4( genD(), genD(), genH(), genP() ) );
			colors.push_back( vec4( palette::paletteRef( genH() * 3.0f, palette::type::paletteIndexed_interpolated ), 1.0f ) );
		}

		rngN trunkJitter( 0.0f, 0.006f );
		rng trunkSizes( 1.75f, 6.36f );
		rng basePtPlace( -0.75f, 0.75f );
		rng leafSizes( 2.27f, 14.6f );
		rngN foliagePlace( 0.0f, 0.1618f );
		for ( int i = 0; i < config.Trees; i++ ) { // trees
			const vec2 basePtOrig = vec2( basePtPlace(), basePtPlace() );
			vec2 basePt = basePtOrig;
			float constrict = 1.618f;
			float scalar = gen();

			config.obstacles.push_back( vec3( basePt.x, basePt.y, 0.06f ) );
			for ( float t = 0; t < scalar; t += 0.002f ) {
				basePt.x += trunkJitter() * 0.5f;
				basePt.y += trunkJitter() * 0.5f;
				constrict *= 0.999f;
				points.push_back( vec4( constrict * trunkJitter() + basePt.x, constrict * trunkJitter() + basePt.y, t, constrict * trunkSizes() ) );
				colors.push_back( vec4( palette::paletteRef( genH(), palette::type::paletteIndexed_interpolated ), gen_normalize() ) );
			}
			for ( int j = 0; j < 1000; j++ ) {
				rngN foliagePlace( 0.0f, 0.1f );
				rngN foliageHeightGen( scalar, 0.05f );
				points.push_back( vec4( basePt.x + foliagePlace(), basePt.y + foliagePlace(), foliageHeightGen(), leafSizes() ) );
				colors.push_back( vec4( palette::paletteRef( genH() + 0.3f, palette::type::paletteIndexed_interpolated ), gen_normalize() ) );
			}
		}

		rngN rockGen( 0.0f, 0.037f );
		rngN rockHGen( 0.06f, 0.037f );
		rngN rockSize( 3.86f, 6.0f );
		for ( int i = 0; i < config.Rocks; i++ ) {
			vec2 basePt = vec2( basePtPlace(), basePtPlace() );
			config.obstacles.push_back( vec3( basePt.x, basePt.y, 0.13f ) );
			for ( int l = 0; l < 1000; l++ ) {
				points.push_back( vec4( basePt.x + rockGen(), basePt.y + rockGen(), rockHGen(), rockSize() ) );
				colors.push_back( vec4( palette::paletteRef( genH() + 0.2f, palette::type::paletteIndexed_interpolated ), gen_normalize() ) );
			}
		}

		// debug spheres for the lights
		if ( config.showLightDebugLocations == true ) {
			for ( unsigned int i = 0; i < lightData.size() / 8; i++ ) {
				const size_t basePt = 8 * i;

				vec4 position = vec4( lightData[ basePt ], lightData[ basePt + 1 ], lightData[ basePt + 2 ], 50.0f );
				// vec4 color = vec4( lights[ basePt + 4 ], lights[ basePt + 5 ], lights[ basePt + 6 ], 1.0f );
				vec4 color = vec4( 1.0f, 0.0f, 0.0f, 1.0f );

				points.push_back( position );
				colors.push_back( color );
			}
		}

		GLuint vao, vbo;
		glGenVertexArrays( 1, &vao );
		glBindVertexArray( vao );
		glGenBuffers( 1, &vbo );
		glBindBuffer( GL_ARRAY_BUFFER, vbo );
		config.numPointsSpheres = points.size();
		size_t numBytesPoints = sizeof( vec4 ) * config.numPointsSpheres;
		size_t numBytesColors = sizeof( vec4 ) * config.numPointsSpheres;
		glBufferData( GL_ARRAY_BUFFER, numBytesPoints + numBytesColors, NULL, GL_STATIC_DRAW );
		glBufferSubData( GL_ARRAY_BUFFER, 0, numBytesPoints, &points[ 0 ] );
		glBufferSubData( GL_ARRAY_BUFFER, numBytesPoints, numBytesColors, &colors[ 0 ] );

		// some set of static points, loaded into the vbo - these won't change, they get generated once, they know where to read from
			// the texture for the height, and then they

		GLuint vPosition = glGetAttribLocation( resources.shaders[ "Sphere" ], "vPosition" );
		glEnableVertexAttribArray( vPosition );
		glVertexAttribPointer( vPosition, 4, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( 0 ) ) );
		GLuint vColor = glGetAttribLocation( resources.shaders[ "Sphere" ], "vColor" );
		glEnableVertexAttribArray( vColor );
		glVertexAttribPointer( vColor, 4, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( numBytesPoints ) ) );

		std::vector< vec4 > ssboPoints;
		int dynamicPointCount = 0;
		rng size( 4.5f, 9.0f );
		rng phase( 0.0f, pi * 2.0f );
		for ( int x = 0; x < config.Guys; x++ ) {
			for ( int y = 0; y < config.Guys; y++ ) {
				ssboPoints.push_back( vec4( 2.0f * ( ( x / float( config.Guys ) ) - 0.5f ), 2.0f * ( ( y / float( config.Guys ) ) - 0.5f ), 0.6f * genH(), size() ) );
				ssboPoints.push_back( vec4( palette::paletteRef( genH() + 0.5f, palette::type::paletteIndexed_interpolated ), phase() ) );
				dynamicPointCount++;
			}
		}
		config.numPointsMovingSpheres = dynamicPointCount;

		GLuint ssbo;
		glGenBuffers( 1, &ssbo );
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ssbo );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * dynamicPointCount, ( GLvoid * ) &ssboPoints[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, ssbo );

		GLuint sphereImage;
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

		resources.textures[ "Sphere Image" ] = sphereImage;
		resources.SSBOs[ "Moving Sphere" ] = ssbo;
		resources.VAOs[ "Sphere" ] = vao;
		resources.VBOs[ "Sphere" ] = vbo;
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

	// todo: deferred pass resources

}

void APIGeometryContainer::InitReport () {
	// tell the stats for the current run of the program
	cout << "\nVertexture2 Init Complete:\n";
	cout << "Point Totals:\n";
	cout << "\tGround:\t\t\t" << config.numPointsGround << newline;
	cout << "\tSkirts:\t\t\t" << config.numPointsSkirts << newline;
	cout << "\tSpheres:\t\t" << config.numPointsSpheres << newline;
	cout << "\tMoving Spheres:\t\t" << config.numPointsMovingSpheres << newline;
	cout << "\tWater:\t\t\t" << config.numPointsWater << newline << newline;
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

	// then the regular view of the geometry
	const mat3 tridentMat = mat3( config.basisX, config.basisY, config.basisZ ); // matrix for the view transform

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
	glBindVertexArray( resources.VAOs[ "Sphere" ] );
	glUseProgram( resources.shaders[ "Sphere" ] );
	glEnable( GL_DEPTH_TEST );
	glEnable( GL_PROGRAM_POINT_SIZE );
	glPointParameteri( GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT );

	// static points
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "time" ), config.timeVal / 10000.0f );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "heightScale" ), config.heightScale );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere" ], "lightCount" ), config.Lights );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "AR" ), config.screenAR );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "frameHeight" ), config.height );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "scale" ), config.scale );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere" ], "heightmap" ), 9 );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere" ], "sphere" ), 10 );
	glUniformMatrix3fv( glGetUniformLocation( resources.shaders[ "Sphere" ], "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );
	glDrawArrays( GL_POINTS, 0, config.numPointsSpheres );

	// dynamic points
	glUseProgram( resources.shaders[ "Moving Sphere" ] );
	glBindImageTexture( 1, resources.textures[ "Steepness Map" ], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F );
	glBindImageTexture( 2, resources.textures[ "Distance/Direction Map" ], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "heightScale" ), config.heightScale );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "time" ), config.timeVal / 10000.0f );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "lightCount" ), config.Lights );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "AR" ), config.screenAR );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "frameHeight" ), config.height );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "scale" ), config.scale );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "heightmap" ), 9 );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "sphere" ), 10 );
	glUniformMatrix3fv( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );
	glDrawArrays( GL_POINTS, 0, config.numPointsMovingSpheres );

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
	rngi gen( 0, 100000 );
	glUseProgram( resources.shaders[ "Sphere Map Update" ] );
	glBindImageTexture( 1, resources.textures[ "Steepness Map" ], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F );
	glBindImageTexture( 2, resources.textures[ "Distance/Direction Map" ], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere Map Update" ], "heightmap" ), 9 );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere Map Update" ], "inSeed" ), gen() );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere Map Update" ], "numObstacles" ), config.obstacles.size() );
	glUniform3fv( glGetUniformLocation( resources.shaders[ "Sphere Map Update" ], "obstacles" ), config.obstacles.size(), glm::value_ptr( config.obstacles[ 0 ] ) );
	glDispatchCompute( 512 / 16, 512 / 16, 1 );

	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	glUseProgram( resources.shaders[ "Sphere Movement" ] );
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, resources.SSBOs[ "Moving Sphere" ] );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, resources.SSBOs[ "Moving Sphere" ] );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere Movement" ], "time" ), config.timeVal / 10000.0f );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere Movement" ], "inSeed" ), gen() );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere Movement" ], "dimension" ), config.Guys );
	glDispatchCompute( config.Guys / 16, config.Guys / 16, 1 ); // dispatch the compute shader to update ssbo

	// run the stuff to update the light positions - this needs more work
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	glUseProgram( resources.shaders[ "Light Movement" ] );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Light Movement" ], "time" ), config.timeVal / 10000.0f );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Light Movement" ], "inSeed" ), gen() );
	glDispatchCompute( config.Lights / 16, 1, 1 ); // dispatch the compute shader to update ssbo

}

void APIGeometryContainer::ControlWindow () {
	ImGui::Begin( "Controls Window", NULL, 0 );

	ImGui::Checkbox( "Show Timing", &config.showTiming );
	ImGui::Checkbox( "Show Trident", &config.showTrident );

	// etc

	ImGui::End();
}
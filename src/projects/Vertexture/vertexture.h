#include <vector>
#include <string>
#include "../../../../src/engine/coreUtils/random.h"
#include "../../../../src/utils/trident/trident.h"
#include "../../../../src/data/colors.h"

// program config
struct vertextureConfig {

	// scaling the size of the GPU resources
	int Guys;
	int Lights;

	// timekeeping since last reset
	float timeVal;

	// visualization parameters
	float scale = 0.3f;
	float heightScale;
	bool showLightDebugLocations;
	vec3 groundColor;

	// output settings
	bool showTiming;
	bool showTrident;

	std::vector< vec3 > obstacles; // x,y location, then radius

	// default orientation
	vec3 basisX;
	vec3 basisY;
	vec3 basisZ;

	// for computing screen AR, from config.json
	uint32_t width;
	uint32_t height;
	float screenAR;

	float layerOffset = 0.0f;
	float layerDepth = 2.0f;

};

struct rnGenerators {
	// these should come from the config, construct everything in the APIGeometryContainer::LoadConfig() function

	rngi shaderWangSeed = rngi( 0, 100000 );

	// needed:
	// 	entity height min, max
	// 	entity phase gen - rngN probably do spread about 0
	// 	dudes max height for the bouncing
	// 	tree height step
	// 		tree number of points in canopy
	// 	tree jitter diameter
	// 		tree diameter for sim should be linked ( * 10.0f, looks like? )
	// 	trunk constrict parameter ( try values >1.0 )
	// 	trunk point size min, max
	// 	canopy point size min, max
	// 	ground cover point size min, max
};

// graphics api resources
struct openGLResources {

	std::unordered_map< string, GLuint > VAOs;
	std::unordered_map< string, GLuint > VBOs;
	std::unordered_map< string, GLuint > SSBOs;
	std::unordered_map< string, GLuint > FBOs;
	std::unordered_map< string, GLuint > textures;
	std::unordered_map< string, GLuint > shaders;

	// static / dynamic point counts, updated and reported during init
	int numPointsSpheres = 0;
	int numPointsMovingSpheres = 0;

	// separate tridents for each shadowmap
	// textures for each shadowmap
	// FBOs for each shadowmap

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
	void DeferredPass (); // do the deferred lighting operations

	// ImGui manipulation of program state
	void ControlWindow ();

	// application data + state
	vertextureConfig config;
	openGLResources resources;
	rnGenerators rngs;

};

void APIGeometryContainer::LoadConfig () {
	// load the config from disk
	json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();

	// this informs the behavior of Initialize()
	config.Guys			= j[ "app" ][ "Vertexture" ][ "Guys" ];
	config.Lights		= j[ "app" ][ "Vertexture" ][ "Lights" ];
	config.showLightDebugLocations = j[ "app" ][ "Vertexture" ][ "ShowLightDebugLocations" ];

		// todo: write out the rest of the parameterization ( point sizes, counts, other distributions... )

	// other program state
	config.showTrident	= j[ "app" ][ "Vertexture" ][ "showTrident" ];
	config.showTiming	= j[ "app" ][ "Vertexture" ][ "showTiming" ];
	// config.scale		= j[ "app" ][ "Vertexture" ][ "InitialScale" ];
	config.heightScale	= j[ "app" ][ "Vertexture" ][ "InitialHeightScale" ];
	config.basisX		= vec3( j[ "app" ][ "Vertexture" ][ "basisX" ][ "x" ], j[ "app" ][ "Vertexture" ][ "basisX" ][ "y" ], j[ "app" ][ "Vertexture" ][ "basisX" ][ "z" ] );
	config.basisY		= vec3( j[ "app" ][ "Vertexture" ][ "basisY" ][ "x" ], j[ "app" ][ "Vertexture" ][ "basisY" ][ "y" ], j[ "app" ][ "Vertexture" ][ "basisY" ][ "z" ] );
	config.basisZ		= vec3( j[ "app" ][ "Vertexture" ][ "basisZ" ][ "x" ], j[ "app" ][ "Vertexture" ][ "basisZ" ][ "y" ], j[ "app" ][ "Vertexture" ][ "basisZ" ][ "z" ] );

	// update AR with current value of screen dims
	config.width = j[ "screenWidth" ];
	config.height = j[ "screenHeight" ];
	config.screenAR = ( float ) config.width / ( float ) config.height;

	// reset amount of time since last reset
	config.timeVal = 0.0f;
}

void APIGeometryContainer::Initialize () {

	// if this is the first time that this has run
		// probably something we can do if we check for this behavior, something - tbd

	// clear existing list of obstacles
	config.obstacles.resize( 0 );

	// create all the graphics api resources
	const string basePath( "./src/projects/Vertexture/shaders/" );
	resources.shaders[ "Background" ]			= computeShader( basePath + "background.cs.glsl" ).shaderHandle;
	resources.shaders[ "Deferred" ]				= computeShader( basePath + "deferred.cs.glsl" ).shaderHandle;
	resources.shaders[ "Sphere Movement" ]		= computeShader( basePath + "movingSphere.cs.glsl" ).shaderHandle;
	resources.shaders[ "Light Movement" ]		= computeShader( basePath + "movingLight.cs.glsl" ).shaderHandle;
	resources.shaders[ "Sphere" ]				= regularShader( basePath + "sphere.vs.glsl", basePath + "sphere.fs.glsl" ).shaderHandle;
	resources.shaders[ "Moving Sphere" ]		= regularShader( basePath + "movingSphere.vs.glsl", basePath + "movingSphere.fs.glsl" ).shaderHandle;

	// GLuint steepness, distanceDirection;
	// Image_4U steepnessTex( 512, 512 );

	// glGenTextures( 1, &steepness );
	// glActiveTexture( GL_TEXTURE14 );
	// glBindTexture( GL_TEXTURE_2D, steepness );
	// glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, steepnessTex.GetImageDataBasePtr() );
	// resources.textures[ "Steepness Map" ] = steepness;

	// glGenTextures( 1, &distanceDirection );
	// glActiveTexture( GL_TEXTURE15 );
	// glBindTexture( GL_TEXTURE_2D, distanceDirection );
	// glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA32F, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, steepnessTex.GetImageDataBasePtr() );
	// resources.textures[ "Distance/Direction Map" ] = distanceDirection;

	// setup the buffers for the rendering process - need depth, worldspace position - maybe?, normals, color
	GLuint primaryFramebuffer;
	glGenFramebuffers( 1, &primaryFramebuffer );
	glBindFramebuffer( GL_FRAMEBUFFER, primaryFramebuffer );

	// create the textures and fill out the framebuffer information
	GLuint fbDepth, fbColor, fbNormal, fbPosition;

	// do the depth texture
	glGenTextures( 1, &fbDepth );
	glActiveTexture( GL_TEXTURE16 );
	glBindTexture( GL_TEXTURE_2D, fbDepth );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, config.width, config.height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
	glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, fbDepth, 0 );

	// do the color texture
	glGenTextures( 1, &fbColor );
	glActiveTexture( GL_TEXTURE17 );
	glBindTexture( GL_TEXTURE_2D, fbColor );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, config.width, config.height, 0, GL_RGBA, GL_FLOAT, NULL );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fbColor, 0 ); // last argument is mip, interesting

	// do the normal texture
	glGenTextures( 1, &fbNormal );
	glActiveTexture( GL_TEXTURE18 );
	glBindTexture( GL_TEXTURE_2D, fbNormal );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, config.width, config.height, 0, GL_RGBA, GL_FLOAT, NULL );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, fbNormal, 0 );

	// do the position texture
	glGenTextures( 1, &fbPosition );
	glActiveTexture( GL_TEXTURE19 );
	glBindTexture( GL_TEXTURE_2D, fbPosition );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, config.width, config.height, 0, GL_RGBA, GL_FLOAT, NULL );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, fbPosition, 0 );

	const GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
	glDrawBuffers( 3, bufs );

	// make sure they're accessible from above
	resources.textures[ "fbDepth" ] = fbDepth;
	resources.textures[ "fbColor" ] = fbColor;
	resources.textures[ "fbNormal" ] = fbNormal;
	resources.textures[ "fbPosition" ] = fbPosition;

	resources.FBOs[ "Primary" ] = primaryFramebuffer;
	if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
		cout << "framebuffer creation successful" << endl;
	}

	// generate all the geometry

	// pick a first palette ( lights )
	palette::PickRandomPalette();

	// randomly positioned + colored lights
		// ssbo, shader for movement
	std::vector< GLfloat > lightData;
	{
		GLuint ssbo;
		rng xDistrib( -10.0f, 10.0f );
		rng colorPick( 0.6f, 0.8f );
		rng brightness( 0.005f, 0.009f );

		for ( int x = 0; x < config.Lights; x++ ) {
		// need to figure out what the buffer needs to hold
			// position ( vec3 + some extra value... we'll find a use for it )
			// color ( vec3 + some extra value, again we'll find some kind of use for it )

			// distribute initial light points
			lightData.push_back( xDistrib() );
			lightData.push_back( xDistrib() );
			lightData.push_back( xDistrib() );
			lightData.push_back( 0.0f );

			vec3 col = palette::paletteRef( colorPick(), palette::type::paletteIndexed_interpolated ) * brightness();
			lightData.push_back( col.r );
			lightData.push_back( col.g );
			lightData.push_back( col.b );
			lightData.push_back( 1.0f );

			// halfway through, switch colors
			if ( x == ( config.Lights / 2 ) ) {
				palette::PickRandomPalette();
			}
		}

		// create the SSBO and bind in slot 4
		glGenBuffers( 1, &ssbo );
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ssbo );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * config.Lights, ( GLvoid * ) &lightData[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 4, ssbo );

		resources.SSBOs[ "Lights" ] = ssbo;
	}

	// pick another palette ( spheres )
	palette::PickRandomPalette();

	// spheres
		// sphere heightmap texture
			// try the math version, see if that's faster? from the nvidia paper
		// shader + vertex buffer of the static points
			// optionally include the debug spheres for the light positions
		// shader + ssbo buffer of the dynamic points
	{
		std::vector< vec4 > points;
		std::vector< vec4 > ssboPoints;
		std::vector< vec4 > colors;

		{
			// some set of static points, loaded into the vbo - these won't change, they get generated once, they know where to read from
				// the texture for the height, and then they displace vertically in the shader

			rng colorGen( 0.0f, 0.65f );
			rng roughnessGen( 0.01f, 1.0f );
			rng di( 3.5f, 16.5f );
			rng size( 0.5f, 4.5f );
			rng phase( 0.0f, pi * 6.0f );
			rng dirPick( -1.0f, 1.0f );
			rng offset( -0.1618f, 0.1618f );
			// rngN anglePick( 0.0f, 0.051616f );
			rng anglePick( -0.1618f, 0.1618f );
			rngi axisPick( 0, 2 );
			rngi cornerPick( 0, 3 );

			const float spreadX = 0.4f;
			const float spreadY = 0.0618f;
			const int numSteps = 75;
			const float stepSize = 0.003f;
			const int detents = 6;
			const float distanceFromCenter = 0.618f;

			for ( int i = 0; i < detents; i++ ) {
				for ( float x = -spreadX; x < spreadX; x += ( 2.0f * spreadX / numSteps ) ) {
					for ( float y = -spreadY; y < spreadY; y += ( 2.0f * spreadX / numSteps ) ) {

						const vec3 startingPoint = glm::rotate( vec3( x, y, distanceFromCenter ), 2.0f * float( pi ) * ( ( float ) i / ( float ) detents ), vec3( 0.0f, 1.0f, 0.0f ) );
						vec3 heading = glm::rotate( vec3( 0.0f, 0.0f, 1.0f ), 2.0f * float( pi ) * ( ( float ) i / ( float ) detents ), vec3( 0.0f, 1.0f, 0.0f ) );
						float diameter = di();

						vec3 currentPoint = startingPoint;

						float magnitude = sin( 1.0f - glm::length( currentPoint.xy() ) * 56.5f );
						vec3 axis = glm::normalize( heading * vec3( dirPick() + offset(), 1.0f + offset(), -magnitude * dirPick() ) );
						vec3 axis2 = glm::normalize( vec3( dirPick(), dirPick(), dirPick() ) );
						const vec3 axeezNuts = glm::cross( axis, axis2 );

						vec4 currentColor = vec4( palette::paletteRef( colorGen() + 0.1f ), roughnessGen() );
						const float segmentLength = 1.4f;

						float decayState = 1.0f;
						const float decayFactor = 0.996f;

						for ( float t = 0.0f; t < segmentLength; t += stepSize ) {
							currentPoint += stepSize * heading;
							points.push_back( vec4( currentPoint, diameter * decayState ) );
							colors.push_back( currentColor );
							// colors.push_back( vec4( 0.1618f, 0.1618f, 0.1618f, 1.0f ) );

							decayState *= decayFactor;
							switch ( axisPick() ) {
							case 0: heading = glm::rotate( heading, anglePick(), axis ); break;
							case 1: heading = glm::rotate( heading, anglePick(), axis2 ); break;
							case 2: heading = glm::rotate( heading, anglePick(), axeezNuts ); break;
							default: break;
							}
						}
					}
				}
				const float rimSize = 18.0f;
				const vec4 darkGrey = vec4( 0.1618f, 0.1618f, 0.1618f, 1.0f );
				const float frameBoost = 1.02f;
				const float distanceOffset = 0.96f;
				const vec3 endpoints[ 4 ] = {
					glm::rotate(
						vec3( -spreadX * frameBoost, -spreadY * frameBoost, distanceFromCenter * distanceOffset ),
						2.0f * float( pi ) * ( ( float ) i / ( float ) detents ),
						vec3( 0.0f, 1.0f, 0.0f ) ),
					glm::rotate(
						vec3( -spreadX * frameBoost,  spreadY * frameBoost, distanceFromCenter * distanceOffset ),
						2.0f * float( pi ) * ( ( float ) i / ( float ) detents ),
						vec3( 0.0f, 1.0f, 0.0f ) ),
					glm::rotate(
						vec3(  spreadX * frameBoost, -spreadY * frameBoost, distanceFromCenter * distanceOffset ),
						2.0f * float( pi ) * ( ( float ) i / ( float ) detents ),
						vec3( 0.0f, 1.0f, 0.0f ) ),
					glm::rotate(
						vec3(  spreadX * frameBoost,  spreadY * frameBoost, distanceFromCenter * distanceOffset ),
						2.0f * float( pi ) * ( ( float ) i / ( float ) detents ),
						vec3( 0.0f, 1.0f, 0.0f ) ),
				};
				{
					const vec3 endpoint0 = endpoints[ 0 ];
					const vec3 endpoint1 = endpoints[ 1 ];
					const float segmentLength = glm::distance( endpoint0, endpoint1 );
					const vec3 heading = normalize( endpoint0 - endpoint1 );
					for ( float t = 0.0f; t < segmentLength; t += stepSize ) {
						points.push_back( vec4( endpoint1 + t * heading, rimSize ) );
						colors.push_back( darkGrey );
					}
				}
				{
					const vec3 endpoint0 = endpoints[ 1 ];
					const vec3 endpoint1 = endpoints[ 3 ];
					const float segmentLength = glm::distance( endpoint0, endpoint1 );
					const vec3 heading = normalize( endpoint0 - endpoint1 );
					for ( float t = 0.0f; t < segmentLength; t += stepSize ) {
						points.push_back( vec4( endpoint1 + t * heading, rimSize ) );
						colors.push_back( darkGrey );
					}
				}
				{
					const vec3 endpoint0 = endpoints[ 2 ];
					const vec3 endpoint1 = endpoints[ 3 ];
					const float segmentLength = glm::distance( endpoint0, endpoint1 );
					const vec3 heading = normalize( endpoint0 - endpoint1 );
					for ( float t = 0.0f; t < segmentLength; t += stepSize ) {
						points.push_back( vec4( endpoint1 + t * heading, rimSize ) );
						colors.push_back( darkGrey );
					}
				}
				{
					const vec3 endpoint0 = endpoints[ 0 ];
					const vec3 endpoint1 = endpoints[ 2 ];
					const float segmentLength = glm::distance( endpoint0, endpoint1 );
					const vec3 heading = normalize( endpoint0 - endpoint1 );
					for ( float t = 0.0f; t < segmentLength; t += stepSize ) {
						points.push_back( vec4( endpoint1 + t * heading, rimSize ) );
						colors.push_back( darkGrey );
					}
				}
			}

			int dynamicPointCount = 0;
			rng pGen( -20.0f, 20.0f );
			for ( int x = 0; x < config.Guys; x++ ) {
				for ( int y = 0; y < config.Guys; y++ ) {
					ssboPoints.push_back( vec4( pGen(), pGen(), pGen(), size() ) );
					ssboPoints.push_back( vec4( palette::paletteRef( colorGen() ), phase() ) );
					dynamicPointCount++;
				}
			}
			resources.numPointsMovingSpheres = dynamicPointCount;
		}

		GLuint vao, vbo;
		glGenVertexArrays( 1, &vao );
		glBindVertexArray( vao );
		glGenBuffers( 1, &vbo );
		glBindBuffer( GL_ARRAY_BUFFER, vbo );
		resources.numPointsSpheres = points.size();
		size_t numBytesPoints = sizeof( vec4 ) * resources.numPointsSpheres;
		size_t numBytesColors = sizeof( vec4 ) * resources.numPointsSpheres;
		glBufferData( GL_ARRAY_BUFFER, numBytesPoints + numBytesColors, NULL, GL_STATIC_DRAW );
		glBufferSubData( GL_ARRAY_BUFFER, 0, numBytesPoints, &points[ 0 ] );
		glBufferSubData( GL_ARRAY_BUFFER, numBytesPoints, numBytesColors, &colors[ 0 ] );

		GLuint vPosition = glGetAttribLocation( resources.shaders[ "Sphere" ], "vPosition" );
		glEnableVertexAttribArray( vPosition );
		glVertexAttribPointer( vPosition, 4, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( 0 ) ) );
		GLuint vColor = glGetAttribLocation( resources.shaders[ "Sphere" ], "vColor" );
		glEnableVertexAttribArray( vColor );
		glVertexAttribPointer( vColor, 4, GL_FLOAT, GL_FALSE, 0, ( ( GLvoid * ) ( numBytesPoints ) ) );

		GLuint ssbo;
		glGenBuffers( 1, &ssbo );
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ssbo );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * resources.numPointsMovingSpheres, ( GLvoid * ) &ssboPoints[ 0 ], GL_DYNAMIC_COPY );
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, ssbo );

		GLuint sphereImage;
		Image_4U sphereImageData( "./src/projects/Vertexture/textures/sphere.png" );
		glGenTextures( 1, &sphereImage );
		glActiveTexture( GL_TEXTURE10 ); // Texture unit 10
		glBindTexture( GL_TEXTURE_2D, sphereImage );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
		glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, sphereImageData.GetImageDataBasePtr() );
		glGenerateMipmap( GL_TEXTURE_2D );

		resources.textures[ "Sphere Image" ] = sphereImage;
		resources.SSBOs[ "Moving Sphere" ] = ssbo;
		resources.VAOs[ "Sphere" ] = vao;
		resources.VBOs[ "Sphere" ] = vbo;
	}

}

void APIGeometryContainer::InitReport () {
	// tell the stats for the current run of the program
	cout << "\nVertexture2 Init Complete:\n";
	cout << "Point Totals:\n";
	cout << "\tSpheres:\t\t" << resources.numPointsSpheres << newline;
	cout << "\tMoving Spheres:\t\t" << resources.numPointsMovingSpheres << newline;
}

void APIGeometryContainer::Terminate () {

	// TODO: destroy the graphics api resources

	glDeleteFramebuffers( 1, &resources.FBOs[ "Primary" ] );

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

void APIGeometryContainer::DeferredPass () {
	glUseProgram( resources.shaders[ "Deferred" ] );

	const mat3 tridentMat = mat3( config.basisX, config.basisY, config.basisZ ); // matrix for the view transform
	glUniformMatrix3fv( glGetUniformLocation( resources.shaders[ "Deferred" ], "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );

	// from glActiveTexture... this sucks, not sure what the correct way is
	glUniform1i( glGetUniformLocation( resources.shaders[ "Deferred" ], "depthTexture" ), 16 );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Deferred" ], "colorTexture" ), 17 );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Deferred" ], "normalTexture" ), 18 );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Deferred" ], "positionTexture" ), 19 );

	glUniform1i( glGetUniformLocation( resources.shaders[ "Deferred" ], "lightCount" ), config.Lights );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Deferred" ], "inSeed" ), rngs.shaderWangSeed() );

	glUniform2f( glGetUniformLocation( resources.shaders[ "Deferred" ], "resolution" ), config.width, config.height );
	glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
}

void APIGeometryContainer::Render () {

	// then the regular view of the geometry
	const mat3 tridentMat = mat3( config.basisX, config.basisY, config.basisZ ); // matrix for the view transform

	glBindFramebuffer( GL_FRAMEBUFFER, resources.FBOs[ "Primary" ] );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	// spheres
	glBindVertexArray( resources.VAOs[ "Sphere" ] );
	glUseProgram( resources.shaders[ "Sphere" ] );
	glEnable( GL_DEPTH_TEST );
	glEnable( GL_PROGRAM_POINT_SIZE );
	glPointParameteri( GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT );

	// static points
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "time" ), config.timeVal / 10000.0f );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere" ], "inSeed" ), rngs.shaderWangSeed() );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere" ], "lightCount" ), config.Lights );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "AR" ), config.screenAR );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "frameWidth" ), config.width );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "scale" ), config.scale );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere" ], "heightmap" ), 9 );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere" ], "sphere" ), 10 );
	glUniformMatrix3fv( glGetUniformLocation( resources.shaders[ "Sphere" ], "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );
	glDrawArrays( GL_POINTS, 0, resources.numPointsSpheres );

	// dynamic points
	glUseProgram( resources.shaders[ "Moving Sphere" ] );
	glBindImageTexture( 1, resources.textures[ "Steepness Map" ], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F );
	glBindImageTexture( 2, resources.textures[ "Distance/Direction Map" ], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "time" ), config.timeVal / 10000.0f );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "inSeed" ), rngs.shaderWangSeed() );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "lightCount" ), config.Lights );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "AR" ), config.screenAR );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "frameWidth" ), config.width );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "scale" ), config.scale );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "heightmap" ), 9 );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "sphere" ), 10 );
	glUniformMatrix3fv( glGetUniformLocation( resources.shaders[ "Moving Sphere" ], "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );
	glDrawArrays( GL_POINTS, 0, resources.numPointsMovingSpheres );

	// revert to default framebuffer
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

void APIGeometryContainer::Update () {

	config.timeVal += 0.1f;

	// // run the stuff to update the moving point locations
	// glUseProgram( resources.shaders[ "Sphere Map Update" ] );
	// glBindImageTexture( 1, resources.textures[ "Steepness Map" ], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F );
	// glBindImageTexture( 2, resources.textures[ "Distance/Direction Map" ], 0, GL_TRUE, 0, GL_READ_WRITE, GL_RGBA32F );
	// glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere Map Update" ], "heightmap" ), 9 );
	// glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere Map Update" ], "inSeed" ), rngs.shaderWangSeed() );
	// glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere Map Update" ], "numObstacles" ), config.obstacles.size() );
	// glUniform3fv( glGetUniformLocation( resources.shaders[ "Sphere Map Update" ], "obstacles" ), config.obstacles.size(), glm::value_ptr( config.obstacles[ 0 ] ) );
	// glDispatchCompute( 512 / 16, 512 / 16, 1 );

	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	glUseProgram( resources.shaders[ "Sphere Movement" ] );
	glBindBuffer( GL_SHADER_STORAGE_BUFFER, resources.SSBOs[ "Moving Sphere" ] );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, resources.SSBOs[ "Moving Sphere" ] );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere Movement" ], "time" ), config.timeVal / 10000.0f );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere Movement" ], "inSeed" ), rngs.shaderWangSeed() );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere Movement" ], "dimension" ), config.Guys );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere Movement" ], "layerDepth"), config.layerDepth );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere Movement" ], "layerOffset"), config.layerOffset );
	glDispatchCompute( config.Guys / 16, config.Guys / 16, 1 ); // dispatch the compute shader to update ssbo

	// run the stuff to update the light positions - this needs more work
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	glUseProgram( resources.shaders[ "Light Movement" ] );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Light Movement" ], "time" ), config.timeVal / 10000.0f );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Light Movement" ], "inSeed" ), rngs.shaderWangSeed() );
	glDispatchCompute( config.Lights / 16, 1, 1 ); // dispatch the compute shader to update ssbo

}

void APIGeometryContainer::ControlWindow () {
	ImGui::Begin( "Controls Window", NULL, 0 );

	ImGui::Checkbox( "Show Timing", &config.showTiming );
	ImGui::Checkbox( "Show Trident", &config.showTrident );

	ImGui::SliderFloat( "Layer Depth", &config.layerDepth, 0.0f, 1.5f, "%.3f" );
	ImGui::SliderFloat( "Layer Offset", &config.layerOffset, 0.0f, 1.5f, "%.3f" );

	// etc

	ImGui::End();
}
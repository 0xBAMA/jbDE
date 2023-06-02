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

	// TODO: add some more stuff on this, to parameterize further - point sizes etc
		// also load from json file, for ability to hot reload
		//  e.g. R to regenerate reads from config file on disk, with all current edits ( don't have to recompile )
			// also do the shaders etc, so we have hot update on that

	// static / dynamic point counts, updated and reported during init
	int groundStaticPointsCount = 0;
	int waterStaticPointsCount = 0;
	int skirtsStaticPointsCount = 0;
	int pointsStaticPointsCount = 0;
	int pointsDynamicPointsCount = 0;
		// ...

	// default orientation
	vec3 basisX = vec3(  0.610246f,  0.454481f,  0.648863f );
	vec3 basisY = vec3(  0.791732f, -0.321969f, -0.519100f );
	vec3 basisZ = vec3( -0.027008f,  0.830518f, -0.556314f );

	// for computing screen AR, from config.json
	uint32_t width = 640;
	uint32_t height = 350;

};

// graphics api resources
struct openGLResources {

	std::unordered_map< string, GLuint > VAOs;
	std::unordered_map< string, GLuint > VBOs;
	std::unordered_map< string, GLuint > SSBOs;
	std::unordered_map< string, GLuint > textures;
	std::unordered_map< string, GLuint > shaders;

	// eventually, framebuffer stuff for deferred work

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
			C ( 2 )	@=======@ D ( 3 ) --> X
		*/

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

	// pick a first palette
		// lights
			// ssbo, shader for movement

	// pick a second palette
		// ground
			// shader, vao / vbo
			// heightmap texture

		// skirts
			// shader, vao / vbo
			// uses heightmap from above

		// spheres
			// sphere heightmap texture
				// try the math version, see if that's faster? from the nvidia paper
			// shader + vertex buffer of the static points
				// optionally include the debug spheres for the light positions
			// shader + ssbo buffer of the dynamic points

		// water
			// shader, vao / vbo
			// 3x textures

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

}

void APIGeometryContainer::Update () {

}
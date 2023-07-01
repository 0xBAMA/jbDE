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
	bool showLightDebugLocations;
	vec3 groundColor;

	float lightsMinBrightness;
	float lightsMaxBrightness;

	// output settings
	bool showTiming;
	bool showTrident;

	std::vector< vec3 > obstacles; // x,y location, then radius

	// default orientation
	vec3 basisX;
	vec3 basisY;
	vec3 basisZ;

	// dynamic points bounds
	float worldX;
	float worldY;

	// for computing screen AR, from config.json
	uint32_t width;
	uint32_t height;
	float screenAR;

	int AONumSamples = 16;
	float AOIntensity = 3.0f;
	float AOScale = 1.0f;
	float AOBias = 0.05f;
	float AOSampleRadius = 0.02f;
	float AOMaxDistance = 0.07f;
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
	int numPointsStaticSpheres = 0;
	int numPointsDynamicSpheres = 0;

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

struct point_t {
	// I think this should be extended:
		// color
		// position
		// ...

	vec4 position;
	vec4 color;
};

struct APIGeometryContainer {

	APIGeometryContainer () {}
	~APIGeometryContainer () {}

	// for keeping track of textures
	textureManager_t * textureManager_local;

	// init
	void LoadConfig ();	// read the relevant section of config.json
	void Initialize ();	// create the shaders, buffers and stuff, random init for points etc
	void Reset () {
		LoadConfig();
		Initialize();
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

	// toggles active framebuffer
	bool swapp = false;

};

void APIGeometryContainer::LoadConfig () {
	// load the config from disk
	json j; ifstream i ( "src/engine/config.json" ); i >> j; i.close();

	// this informs the behavior of Initialize()
	config.Lights		= j[ "app" ][ "Vertexture" ][ "Lights" ];

	// other program state
	config.showTrident	= j[ "app" ][ "Vertexture" ][ "showTrident" ];
	config.showTiming	= j[ "app" ][ "Vertexture" ][ "showTiming" ];
	config.lightsMinBrightness = j[ "app" ][ "Vertexture" ][ "Lights Min Brightness" ];
	config.lightsMaxBrightness = j[ "app" ][ "Vertexture" ][ "Lights Max Brightness" ];
	config.worldX		= j[ "app" ][ "Vertexture" ][ "worldX" ];
	config.worldY		= j[ "app" ][ "Vertexture" ][ "worldY" ];

	// update AR with current value of screen dims
	config.width = j[ "screenWidth" ];
	config.height = j[ "screenHeight" ];
	config.screenAR = ( float ) config.width / ( float ) config.height;

	// reset amount of time since last reset
	config.timeVal = 0.0f;
}

void APIGeometryContainer::Initialize () {
	std::vector< point_t > points;

	// clear existing list of obstacles
	config.obstacles.resize( 0 );

	// create all the graphics api resources
	const string basePath( "./src/projects/Vertexture/shaders/" );
	resources.shaders[ "Background" ]		= computeShader( basePath + "background.cs.glsl" ).shaderHandle;
	resources.shaders[ "Deferred" ]			= computeShader( basePath + "deferred.cs.glsl" ).shaderHandle;
	resources.shaders[ "Sphere Movement" ]	= computeShader( basePath + "sphereMove.cs.glsl" ).shaderHandle;
	resources.shaders[ "Light Movement" ]	= computeShader( basePath + "lightMove.cs.glsl" ).shaderHandle;
	resources.shaders[ "Sphere" ]			= regularShader( basePath + "sphere.vs.glsl", basePath + "sphere.fs.glsl" ).shaderHandle;

	static bool firstTime = true;
	if ( firstTime ) {

		textureOptions_t opts;

		// ==== Depth =========================
		opts.dataType = GL_DEPTH_COMPONENT32;
		opts.textureType = GL_TEXTURE_2D;
		opts.width = config.width;
		opts.height = config.height;
		textureManager_local->Add( "Framebuffer Depth 0", opts );
		textureManager_local->Add( "Framebuffer Depth 1", opts );
		textureManager_local->Add( "Geometry Framebuffer Depth", opts );
		// ====================================

		// ==== Normal =======================
		opts.dataType = GL_RGBA16F;
		textureManager_local->Add( "Framebuffer Normal 0", opts );
		textureManager_local->Add( "Framebuffer Normal 1", opts );
		textureManager_local->Add( "Geometry Framebuffer Normal", opts );
		// ====================================

		// ==== Position ======================
		textureManager_local->Add( "Framebuffer Position 0", opts );
		textureManager_local->Add( "Framebuffer Position 1", opts );
		textureManager_local->Add( "Geometry Framebuffer Position", opts );
		// ====================================

		// ==== Material ID ===================
		opts.dataType = GL_RG32UI;
		textureManager_local->Add( "Framebuffer Material ID 0", opts );
		textureManager_local->Add( "Framebuffer Material ID 1", opts );
		// ====================================

		// ==== Albedo ========================
		opts.dataType = GL_RGB8;
		textureManager_local->Add( "Geometry Framebuffer Albedo", opts );
		// ====================================

		// setup the buffers for the rendering process
		GLuint primaryFramebuffer[ 3 ];
		glGenFramebuffers( 3, &primaryFramebuffer[ 0 ] );

		// all framebuffers have depth + 3 color attachments
		const GLenum bufs[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };

		// creating the actual framebuffers with their attachments
		glBindFramebuffer( GL_FRAMEBUFFER, primaryFramebuffer[ 0 ] );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureManager_local->Get( "Framebuffer Depth 0" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureManager_local->Get( "Framebuffer Normal 0" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, textureManager_local->Get( "Framebuffer Position 0" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, textureManager_local->Get( "Framebuffer Material ID 0" ), 0 );
		glDrawBuffers( 3, bufs ); // how many active attachments, and their attachment locations
		if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
			cout << "front framebuffer creation successful" << endl;
		}

		glBindFramebuffer( GL_FRAMEBUFFER, primaryFramebuffer[ 1 ] );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureManager_local->Get( "Framebuffer Depth 1" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureManager_local->Get( "Framebuffer Normal 1" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, textureManager_local->Get( "Framebuffer Position 1" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, textureManager_local->Get( "Framebuffer Material ID 1" ), 0 );
		glDrawBuffers( 3, bufs );
		if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
			cout << "back framebuffer creation successful" << endl;
		}

		// geometry framebuffer needs:
			// albedo -> texture reference, so material ID not useful
			// normal
			// position

		// also - maybe do this at a lower resolution - I think this makes sense, so that point density doesn't get too too high

		glBindFramebuffer( GL_FRAMEBUFFER, primaryFramebuffer[ 2 ] );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureManager_local->Get( "Geometry Framebuffer Depth" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureManager_local->Get( "Geometry Framebuffer Normal" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, textureManager_local->Get( "Geometry Framebuffer Position" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, textureManager_local->Get( "Geometry Framebuffer Albedo" ), 0 );
		glDrawBuffers( 3, bufs );
		if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
			cout << "geometry framebuffer creation successful" << endl;
		}

		resources.FBOs[ "Primary0" ] = primaryFramebuffer[ 0 ];
		resources.FBOs[ "Primary1" ] = primaryFramebuffer[ 1 ];
		resources.FBOs[ "Geometry" ] = primaryFramebuffer[ 2 ];
	}

// generating the geometry

	// pick a palette ( for the lights )
	palette::PickRandomPalette();

	// randomly positioned + colored lights
		// ssbo, shader for movement
	std::vector< GLfloat > lightData;
	{
		GLuint ssbo;
		rng xDistrib( -10.0f, 10.0f );
		rng colorPick( 0.6f, 0.8f );
		rng brightness( config.lightsMinBrightness, config.lightsMaxBrightness );

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

		glGenBuffers( 1, &ssbo );
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ssbo );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( GLfloat ) * 8 * config.Lights, ( GLvoid * ) &lightData[ 0 ], GL_DYNAMIC_COPY );

		resources.SSBOs[ "Lights" ] = ssbo;
	}

	{
		points.resize( 1000 );

		GLuint ssbo;
		glGenBuffers( 1, &ssbo );
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ssbo );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( point_t ) * ( points.size() ), ( GLvoid * ) &points[ 0 ], GL_DYNAMIC_COPY );

		resources.SSBOs[ "Spheres" ] = ssbo;
	}

	// bind in known locations
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, resources.SSBOs[ "Spheres" ] );
	glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 4, resources.SSBOs[ "Lights" ] );
}

void APIGeometryContainer::Terminate () {

	// TODO: destroy the graphics api resources

	// textures
	// ssbos
	// ...

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
	const GLuint shader = resources.shaders[ "Deferred" ];
	glUseProgram( shader );

	const mat3 tridentMat = mat3( config.basisX, config.basisY, config.basisZ ); // matrix for the view transform
	glUniformMatrix3fv( glGetUniformLocation( shader, "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );

	textureManager_local->BindTexForShader( string( "Framebuffer Depth " ) + string( swapp ? "1" : "0" ), "depthTexture", shader, 0 );
	textureManager_local->BindTexForShader( string( "Framebuffer Depth " ) + string( swapp ? "0" : "1" ), "depthTexturePrevious", shader, 1 );
	textureManager_local->BindTexForShader( string( "Framebuffer Normal " ) + string( swapp ? "1" : "0" ), "normalTexture", shader, 2 );
	textureManager_local->BindTexForShader( string( "Framebuffer Normal " ) + string( swapp ? "0" : "1" ), "normalTexturePrevious", shader, 3 );
	textureManager_local->BindTexForShader( string( "Framebuffer Position " ) + string( swapp ? "1" : "0" ), "positionTexture", shader, 4 );
	textureManager_local->BindTexForShader( string( "Framebuffer Position " ) + string( swapp ? "0" : "1" ), "positionTexturePrevious", shader, 5 );
	textureManager_local->BindTexForShader( string( "Framebuffer Material ID " ) + string( swapp ? "1" : "0" ), "idTexture", shader, 6 );
	textureManager_local->BindTexForShader( string( "Framebuffer Material ID " ) + string( swapp ? "0" : "1" ), "idTexturePrevious", shader, 7 );

	// SSAO config
	glUniform1i( glGetUniformLocation( shader, "AONumSamples" ), config.AONumSamples );
	glUniform1f( glGetUniformLocation( shader, "AOIntensity" ), config.AOIntensity );
	glUniform1f( glGetUniformLocation( shader, "AOScale" ), config.AOScale );
	glUniform1f( glGetUniformLocation( shader, "AOBias" ), config.AOBias );
	glUniform1f( glGetUniformLocation( shader, "AOSampleRadius" ), config.AOSampleRadius );
	glUniform1f( glGetUniformLocation( shader, "AOMaxDistance" ), config.AOMaxDistance );

	// Lights, rng, resolution
	glUniform1i( glGetUniformLocation( shader, "lightCount" ), config.Lights );
	glUniform1i( glGetUniformLocation( shader, "inSeed" ), rngs.shaderWangSeed() );
	glUniform2f( glGetUniformLocation( shader, "resolution" ), config.width, config.height );

	glDispatchCompute( ( config.width + 15 ) / 16, ( config.height + 15 ) / 16, 1 );
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );

	// swapt front and back buffers
	swapp = !swapp;
}

void APIGeometryContainer::Render () {

	// then the regular view of the geometry
	const mat3 tridentMat = mat3( config.basisX, config.basisY, config.basisZ ); // matrix for the view transform

	const mat4 perspectiveMatrix = glm::perspective( 45.0f, ( GLfloat ) config.width / ( GLfloat ) config.height, 0.1f, 5.0f );
	const mat4 lookatMatrix = glm::lookAt( vec3( 2.0f, 0.0f, 0.0f ), vec3( 0.0f ), vec3( 0.0f, 1.0f, 0.0f ) );

	// bind the current framebuffer and clear it
	glBindFramebuffer( GL_FRAMEBUFFER, swapp ? resources.FBOs[ "Primary1" ] : resources.FBOs[ "Primary0" ] );
	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

	// unified spheres
	glEnable( GL_DEPTH_TEST );
	glEnable( GL_PROGRAM_POINT_SIZE );
	glPointParameteri( GL_POINT_SPRITE_COORD_ORIGIN, GL_LOWER_LEFT );

	// static points
	glUseProgram( resources.shaders[ "Sphere" ] );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "time" ), config.timeVal / 10000.0f );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere" ], "inSeed" ), rngs.shaderWangSeed() );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "AR" ), config.screenAR );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "frameWidth" ), config.width );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere" ], "scale" ), config.scale );
	glUniformMatrix3fv( glGetUniformLocation( resources.shaders[ "Sphere" ], "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );
	glUniformMatrix4fv( glGetUniformLocation( resources.shaders[ "Sphere" ], "perspectiveMatrix" ), 1, GL_FALSE, glm::value_ptr( perspectiveMatrix ) );
	glUniformMatrix4fv( glGetUniformLocation( resources.shaders[ "Sphere" ], "lookatMatrix" ), 1, GL_FALSE, glm::value_ptr( lookatMatrix ) );

	// gl_VertexID is informed by these values, and is continuous - first call starts at zero, second call starts at resources.numPointsStaticSpheres
	int start, number;

	// this will need to be revisited
	start = 0;
	number = resources.numPointsStaticSpheres;
	glDrawArrays( GL_POINTS, start, number ); // draw start to numstatic

	start = resources.numPointsStaticSpheres;
	number = resources.numPointsDynamicSpheres;
	glDrawArrays( GL_POINTS, start, number ); // draw numstatic to end

	// revert to default framebuffer
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

void APIGeometryContainer::Update () {
	config.timeVal += 0.1f;

	// glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	// glUseProgram( resources.shaders[ "Sphere Movement" ] );
	// glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere Movement" ], "time" ), config.timeVal / 10000.0f );
	// glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere Movement" ], "inSeed" ), rngs.shaderWangSeed() );
	// glUniform1i( glGetUniformLocation( resources.shaders[ "Sphere Movement" ], "dimension" ), config.Guys );
	// glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere Movement" ], "worldX"), config.worldX );
	// glUniform1f( glGetUniformLocation( resources.shaders[ "Sphere Movement" ], "worldY"), config.worldY );
	// glDispatchCompute( config.Guys / 16, config.Guys / 16, 1 ); // dispatch the compute shader to update ssbo

	// run the stuff to update the light positions - this needs more work
	glMemoryBarrier( GL_SHADER_IMAGE_ACCESS_BARRIER_BIT );
	glUseProgram( resources.shaders[ "Light Movement" ] );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Light Movement" ], "time" ), config.timeVal / 10000.0f );
	glUniform1i( glGetUniformLocation( resources.shaders[ "Light Movement" ], "inSeed" ), rngs.shaderWangSeed() );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Light Movement" ], "worldX"), config.worldX );
	glUniform1f( glGetUniformLocation( resources.shaders[ "Light Movement" ], "worldY"), config.worldY );
	glDispatchCompute( config.Lights / 16, 1, 1 ); // dispatch the compute shader to update ssbo

}

void APIGeometryContainer::ControlWindow () {
	ImGui::Begin( "Controls Window", NULL, 0 );

	ImGui::Checkbox( "Show Timing", &config.showTiming );
	ImGui::Checkbox( "Show Trident", &config.showTrident );

	ImGui::Text( "Wrapping" );
	ImGui::SliderFloat( "worldY", &config.worldX, 0.0f, 4.5f, "%.3f" );
	ImGui::SliderFloat( "worldX", &config.worldY, 0.0f, 4.5f, "%.3f" );

	ImGui::Text( "AO" );
	ImGui::SliderInt( "AO Samples", &config.AONumSamples, 0, 64 );
	ImGui::SliderFloat( "AO Intensity", &config.AOIntensity, 0.0f, 10.0f, "%.3f");
	ImGui::SliderFloat( "AO Scale", &config.AOScale, 0.0f, 10.0f, "%.3f");
	ImGui::SliderFloat( "AO Bias", &config.AOBias, 0.0f, 0.1f, "%.3f");
	ImGui::SliderFloat( "AO Sample Radius", &config.AOSampleRadius, 0.0f, 0.05f, "%.3f");
	ImGui::SliderFloat( "AO Max Distance", &config.AOMaxDistance, 0.0f, 0.14f, "%.3f");

	ImGui::End();
}
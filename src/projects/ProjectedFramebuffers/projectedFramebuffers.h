#include <vector>
#include <string>
#include "../../../../src/engine/coreUtils/random.h"
#include "../../../../src/utils/trident/trident.h"
#include "../../../../src/data/colors.h"

// program config
struct vertextureConfig {

	// scaling the size of the GPU resources
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
	uint32_t framebufferX;
	uint32_t framebufferY;
	float screenAR;

	int AONumSamples = 16;
	float AOIntensity = 3.0f;
	float AOScale = 1.0f;
	float AOBias = 0.05f;
	float AOSampleRadius = 0.02f;
	float AOMaxDistance = 0.07f;
};

// holding any required rng objects
struct rnGenerators {
	rngi shaderWangSeed	= rngi( 0, 100000 );
	rng xDistrib		= rng( -10.0f, 10.0f );
	rng colorPick		= rng( 0.6f, 0.8f );
	rng brightness		= rng( 0.0f, 1.0f );
	rng gen				= rng( -1.0f, 1.0f );
	rng size			= rng( 4.0f, 25.0f );
	rng color			= rng( 0.0f, 0.65f );
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
	config.Lights				= j[ "app" ][ "ProjectedFramebuffers" ][ "Lights" ];

	// other program state
	config.showTrident			= j[ "app" ][ "ProjectedFramebuffers" ][ "showTrident" ];
	config.showTiming			= j[ "app" ][ "ProjectedFramebuffers" ][ "showTiming" ];
	config.lightsMinBrightness	= j[ "app" ][ "ProjectedFramebuffers" ][ "Lights Min Brightness" ];
	config.lightsMaxBrightness	= j[ "app" ][ "ProjectedFramebuffers" ][ "Lights Max Brightness" ];
	config.worldX				= j[ "app" ][ "ProjectedFramebuffers" ][ "worldX" ];
	config.worldY				= j[ "app" ][ "ProjectedFramebuffers" ][ "worldY" ];
	config.framebufferX			= j[ "app" ][ "ProjectedFramebuffers" ][ "framebufferX" ];
	config.framebufferY			= j[ "app" ][ "ProjectedFramebuffers" ][ "framebufferY" ];

	// populate from config
	rngs.brightness = rng( config.lightsMinBrightness, config.lightsMaxBrightness );

	// update AR with current value of screen dims
	config.width				= j[ "screenWidth" ];
	config.height				= j[ "screenHeight" ];
	config.screenAR 			= ( float ) config.width / ( float ) config.height;

	// reset amount of time since last reset
	config.timeVal = 0.0f;
}

void APIGeometryContainer::Initialize () {
	std::vector< point_t > points;

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

		// == Render Framebuffer ==============
		// ==== Depth =========================
		opts.dataType = GL_DEPTH_COMPONENT32;
		opts.textureType = GL_TEXTURE_2D;
		opts.width = config.width;
		opts.height = config.height;
		textureManager_local->Add( "Render Framebuffer Depth 0", opts );
		textureManager_local->Add( "Render Framebuffer Depth 1", opts );
		// ==== Normal =======================
		opts.dataType = GL_RGBA16F;
		textureManager_local->Add( "Render Framebuffer Normal 0", opts );
		textureManager_local->Add( "Render Framebuffer Normal 1", opts );
		// ==== Position ======================
		textureManager_local->Add( "Render Framebuffer Position 0", opts );
		textureManager_local->Add( "Render Framebuffer Position 1", opts );
		// ==== Material ID ===================
		opts.dataType = GL_RG32UI;
		textureManager_local->Add( "Render Framebuffer Material ID 0", opts );
		textureManager_local->Add( "Render Framebuffer Material ID 1", opts );
		// ====================================

		// == Geometry Framebuffer ============
		// ==== Depth ========================
		opts.dataType = GL_DEPTH_COMPONENT32;
		opts.width = config.framebufferX;
		opts.height = config.framebufferY;
		textureManager_local->Add( "Geometry Framebuffer Depth", opts );
		// ==== Normal =======================
		opts.dataType = GL_RGBA16F;
		textureManager_local->Add( "Geometry Framebuffer Normal", opts );
		// ==== Position ======================
		textureManager_local->Add( "Geometry Framebuffer Position", opts );
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
		glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureManager_local->Get( "Render Framebuffer Depth 0" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureManager_local->Get( "Render Framebuffer Normal 0" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, textureManager_local->Get( "Render Framebuffer Position 0" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, textureManager_local->Get( "Render Framebuffer Material ID 0" ), 0 );
		glDrawBuffers( 3, bufs ); // how many active attachments, and their attachment locations
		if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE ) {
			cout << "front framebuffer creation successful" << endl;
		}

		glBindFramebuffer( GL_FRAMEBUFFER, primaryFramebuffer[ 1 ] );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, textureManager_local->Get( "Render Framebuffer Depth 1" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, textureManager_local->Get( "Render Framebuffer Normal 1" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, textureManager_local->Get( "Render Framebuffer Position 1" ), 0 );
		glFramebufferTexture( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, textureManager_local->Get( "Render Framebuffer Material ID 1" ), 0 );
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

		for ( int x = 0; x < config.Lights; x++ ) {
		// need to figure out what the buffer needs to hold
			// position ( vec3 + some extra value... we'll find a use for it )
			// color ( vec3 + some extra value, again we'll find some kind of use for it )

			// distribute initial light points
			lightData.push_back( rngs.xDistrib() );
			lightData.push_back( rngs.xDistrib() );
			lightData.push_back( rngs.xDistrib() );
			lightData.push_back( 0.0f );

			vec3 col = palette::paletteRef( rngs.colorPick(), palette::type::paletteIndexed_interpolated ) * rngs.brightness();
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
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 4, resources.SSBOs[ "Lights" ] );
	}

	{
		// put in some placeholder shit, to show signs of life


		const uint32_t numPixelsFramebuffer = config.framebufferX * config.framebufferY;
		for ( uint32_t i = 0; i < numPixelsFramebuffer; i++ ) {
			points.push_back( {
				vec4( rngs.gen(), rngs.gen(), rngs.gen(), rngs.size() ),
				vec4( palette::paletteRef( rngs.color() ), 0.0f )
			} );
		}

		GLuint ssbo;
		glGenBuffers( 1, &ssbo );
		glBindBuffer( GL_SHADER_STORAGE_BUFFER, ssbo );
		glBufferData( GL_SHADER_STORAGE_BUFFER, sizeof( point_t ) * ( points.size() ), ( GLvoid * ) &points[ 0 ], GL_DYNAMIC_COPY );
		resources.SSBOs[ "Spheres" ] = ssbo;

		// need to know how many there are for later
		resources.numPointsStaticSpheres = points.size();
		glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, resources.SSBOs[ "Spheres" ] );

	}

	{
		// the sponza geometry

		// the array texture
	}

}

void APIGeometryContainer::Terminate () {

	// TODO: destroy the graphics api resources

	// textures
	// ssbos
	// ...

	glDeleteFramebuffers( 1, &resources.FBOs[ "Primary" ] );

}

void APIGeometryContainer::DeferredPass () {
	const GLuint shader = resources.shaders[ "Deferred" ];
	glUseProgram( shader );

	const mat3 tridentMat = mat3( config.basisX, config.basisY, config.basisZ ); // matrix for the view transform
	glUniformMatrix3fv( glGetUniformLocation( shader, "trident" ), 1, GL_FALSE, glm::value_ptr( tridentMat ) );

	textureManager_local->BindTexForShader( string( "Render Framebuffer Depth " ) + string( swapp ? "1" : "0" ), "depthTexture", shader, 0 );
	textureManager_local->BindTexForShader( string( "Render Framebuffer Depth " ) + string( swapp ? "0" : "1" ), "depthTexturePrevious", shader, 1 );
	textureManager_local->BindTexForShader( string( "Render Framebuffer Normal " ) + string( swapp ? "1" : "0" ), "normalTexture", shader, 2 );
	textureManager_local->BindTexForShader( string( "Render Framebuffer Normal " ) + string( swapp ? "0" : "1" ), "normalTexturePrevious", shader, 3 );
	textureManager_local->BindTexForShader( string( "Render Framebuffer Position " ) + string( swapp ? "1" : "0" ), "positionTexture", shader, 4 );
	textureManager_local->BindTexForShader( string( "Render Framebuffer Position " ) + string( swapp ? "0" : "1" ), "positionTexturePrevious", shader, 5 );
	textureManager_local->BindTexForShader( string( "Render Framebuffer Material ID " ) + string( swapp ? "1" : "0" ), "idTexture", shader, 6 );
	textureManager_local->BindTexForShader( string( "Render Framebuffer Material ID " ) + string( swapp ? "0" : "1" ), "idTexturePrevious", shader, 7 );

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

	// render the geometry into the geometry framebuffer

	// populate the SSBO with the points

	// then the render the points, out of the SSBO
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

	// revert to default framebuffer
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
}

void APIGeometryContainer::Update () {
	config.timeVal += 0.1f;

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
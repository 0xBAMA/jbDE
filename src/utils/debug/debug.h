#ifndef DEBUG
#define DEBUG

#include "../../engine/includes.h"

static int numMsDelayAfterCallback = 100;

//gl debug dump
void GLAPIENTRY MessageCallback ( 	GLenum source,
						GLenum type,
						GLuint id,
						GLenum severity,
						GLsizei length,
						const GLchar* message,
						const void* userParam ) {

	const bool show_high_severity = true;
	const bool show_medium_severity = true;
	const bool show_low_severity = true;
	const bool show_notification_severity = false;

	const char* errorText = ( type == GL_DEBUG_TYPE_ERROR ) ? "** GL ERROR **" : "";

	switch ( severity ) {
		case GL_DEBUG_SEVERITY_HIGH:
			if ( show_high_severity )
				fprintf( stderr, "\t GL CALLBACK: %s type: 0x%x, severity: HIGH, message: %s\n", errorText, type, message );
			break;
		case GL_DEBUG_SEVERITY_MEDIUM:
			if ( show_medium_severity )
				fprintf( stderr, "\t GL CALLBACK: %s type: 0x%x, severity: MEDIUM, message: %s\n", errorText, type, message );
			break;
		case GL_DEBUG_SEVERITY_LOW:
			if ( show_low_severity )
				fprintf( stderr, "\t GL CALLBACK: %s type: 0x%x, severity: LOW, message: %s\n", errorText, type, message );
			break;
		case GL_DEBUG_SEVERITY_NOTIFICATION:
			if ( show_notification_severity )
				fprintf( stderr, "\t GL CALLBACK: %s type: 0x%x, severity: NOTIFICATION, message: %s\n", errorText, type, message );
			break;
	}
	SDL_Delay( numMsDelayAfterCallback ); // hang a short time so the spew doesn't make it impossible to get back to the error
}

void GLDebugEnable () {
	//DEBUG ENABLE
	glEnable( GL_DEBUG_OUTPUT );
	glDebugMessageCallback( MessageCallback, 0 );

	//report all gl extensions - useful on different platforms
	// GLint n;
	// glGetIntegerv( GL_NUM_EXTENSIONS, &n );
	// cout << "starting dump of " << n << " extensions" << endl;
	// for ( int i = 0; i < n; i++ )
	//   cout << i << ": " << glGetStringi( GL_EXTENSIONS, i ) << endl;
	// cout << endl;

	//gl info re:texture size, texture units
	// GLint val;
	// glGetIntegerv( GL_MAX_TEXTURE_SIZE, &val );
	// cout << "max texture size reports: " << val << endl << endl;
	// glGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &val );
	// cout << "max 3dtexture size reports: " << val << " on all 3 edges" << endl << endl;
	// glGetIntegerv( GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, &val );
	// cout << "max compute texture image units reports: " << val << endl << endl;
}

#endif

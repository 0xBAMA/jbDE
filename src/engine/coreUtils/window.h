#ifndef WINDOWHANDLER_H
#define WINDOWHANDLER_H
#include "../includes.h"

class windowHandler {
public:
	windowHandler () {}

	void PreInit () {
		if ( SDL_Init( SDL_INIT_EVERYTHING ) != 0 ) {
			cout << "Error: " << SDL_GetError() << newline;
		}
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER,       1 );
		SDL_GL_SetAttribute( SDL_GL_ACCELERATED_VISUAL, 1 );
		SDL_GL_SetAttribute( SDL_GL_RED_SIZE,           8 );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE,         8 );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE,          8 );
		SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE,         8 );
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE,        24 );
		SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE,       8 );
		// multisampling AA, for edges when evaluating API geometry
		if ( config->MSAACount > 1 ) {
			SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 1 );
			SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, config->MSAACount );
		}
		vsyncState = config->vSyncEnable;
	}

	void Init () {
		// prep for window creation
		SDL_DisplayMode displayMode;
		SDL_GetDesktopDisplayMode( 0, &displayMode );

		// 0 or negative numbers will size the window relative to the display
		config->width = ( config->width <= 0 ) ? displayMode.w + config->width : config->width;
		config->height = ( config->height <= 0 ) ? displayMode.h + config->height : config->height;

		// always need OpenGL, always start hidden till init finishes
		config->windowFlags |= SDL_WINDOW_OPENGL;
		config->windowFlags |= SDL_WINDOW_HIDDEN;
		window = SDL_CreateWindow( config->windowTitle.c_str(), config->windowOffset.x + config->startOnScreen * displayMode.w,
			config->windowOffset.y, config->width, config->height, config->windowFlags );
	}

	void OpenGLSetup () {
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, 0 );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
		// defaults to OpenGL 4.3
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, config->OpenGLVersionMajor );
		SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, config->OpenGLVersionMinor );
		GLcontext = SDL_GL_CreateContext( window );
		SDL_GL_MakeCurrent( window, GLcontext );

		// config vsync enable/disable
		SDL_GL_SetSwapInterval( config->vSyncEnable ? 1 : 0 );

		// load OpenGL functions
		if ( glewInit() != GLEW_OK ) { cout << "Failed to Initialize OpenGL Loader!" << newline; abort(); }

		// basic OpenGL Config
		glEnable( GL_DEPTH_TEST );
		// glEnable( GL_LINE_SMOOTH );
		// glPointSize( 3.0 );
		glEnable( GL_BLEND );
		glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	}

	void ToggleVSync () {
		vsyncState = !vsyncState;
		SDL_GL_SetSwapInterval( vsyncState ? 1 : 0 );
	}

	void Swap () {
		SDL_GL_SwapWindow( window );
	}

	void Kill () {
		SDL_GL_DeleteContext( GLcontext );
		SDL_DestroyWindow( window );
		SDL_Quit();
	}

	bool vsyncState = true;
	uint32_t flags;
	SDL_Window * window;
	SDL_GLContext GLcontext;
	configData * config;
};

#endif
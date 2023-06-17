#pragma once
#ifndef TEXTURE_H
#define TEXTURE_H

#include "../includes.h"

//===== Texture Options ===============================================================================================
struct textureOptions_t {

	// what do we need to know about the texture?
		// type
			// data type
			// texture type
		// dimensions
			// x, y, z, layers
		// filtering
			// min, mag
		// wrap mode
		// initial data ( optional pointer to Image2 or nullptr for null init )
		// ...

};


//===== Texture Record ================================================================================================
struct texture_t {

	// used to store information about active textures
	textureOptions_t creationOptions;

	GLenum textureUnit; // GL_TEXTURE0 + N

};

//===== Texture Manager ===============================================================================================
class textureManager_t {
public:
	void Init () {

		// report some platform detials:
			// number of available texture units
			// maximum dimensions of texture
			// ...

		GLint val;
		glGetIntegerv( GL_MAX_TEXTURE_SIZE, &val );
		cout << endl << "max texture size reports: " << val << endl;

		glGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &val );
		cout << "max 3dtexture size reports: " << val << endl;

		glGetIntegerv( GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, &val );
		cout << "max compute texture image units reports: " << val << endl;

		glGetIntegerv( GL_MAX_DRAW_BUFFERS, &val );
		cout << "max draw buffers: " << val << endl;

		glGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &val );
		cout << "max combined texture image units: " << val << endl << endl;

	}

	void Add ( string label, textureOptions_t &texOptsIn ) {

		// increment count on add, so we can use this for glActiveTexture( GL_TEXTURE0 + count )
		static int count = 0;
		count++;

	}

	std::unordered_map< string, texture_t > textures;
};

#endif // TEXTURE_H
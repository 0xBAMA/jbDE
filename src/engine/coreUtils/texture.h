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

	GLuint textureHandle; // created texture handle
	GLenum textureUnit;  // GL_TEXTURE0 + N
	size_t textureSize; // required storage space

};

//===== Texture Manager ===============================================================================================
class textureManager_t {
public:

	void Init () {

		// add config toggle for this eventually

		// report some platform detials:
			// number of available texture units
			// maximum dimensions of texture
			// ...

		// GLint val;
		// glGetIntegerv( GL_MAX_TEXTURE_SIZE, &val );
		// cout << endl << endl << "\t\tMax Texture Size Reports: " << val << endl;

		// glGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &val );
		// cout << "\t\tMax 3D Texture Size Reports: " << val << endl;

		// glGetIntegerv( GL_MAX_DRAW_BUFFERS, &val );
		// cout << "\t\tMax Draw Buffers: " << val << endl;

		// glGetIntegerv( GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, &val );
		// cout << "\t\tMax Compute Texture Image Units Reports: " << val << endl;

		// glGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &val );
		// cout << "\t\tMax Combined Texture Image Units: " << val << endl << endl;

		// // make up for the spacing issues
		// cout << "\t............................................................. ";

	}

	int count = 0;
	void Add ( string label, textureOptions_t &texOptsIn ) {

		

		// increment count on add, so we can use this for glActiveTexture( GL_TEXTURE0 + count )
		count++;
	}

	GLuint Get ( string label ) { // if the map contains the key, return it, else some nonsense value
		return ( textures.find( label ) != textures.end() ) ? textures[ label ].textureHandle : std::numeric_limits< GLuint >::max();
	}

	// total size in bytes
	size_t TotalSize () {
		size_t total = 0;
		for ( auto& tex : textures )
			total += tex.second.textureSize;
		return total;
	}

	std::unordered_map< string, texture_t > textures;
};

#endif // TEXTURE_H
#pragma once
#ifndef TEXTURE_H
#define TEXTURE_H

#include "../includes.h"

//===== Helper Functions ==============================================================================================
inline size_t bytesPerPixel ( GLint type ) {
	// over time we'll accumulate all the ones that I use, these are the current set
	switch ( type ) {
	case GL_DEPTH_COMPONENT32:	return 1 * 4; break;
	case GL_RG32UI:				return 2 * 4; break;
	case GL_RGBA8:				return 4 * 1; break;
	case GL_RGBA16F:			return 4 * 2; break;
	default:
		cout << "unknown type texture created" << endl;
		return 0;
		break;
	}
}

inline GLenum getFormat( GLint internalFormat ) {

}

//===== Texture Options ===============================================================================================
// what do you need to know to create the texture?
struct textureOptions_t {
	// data type
	GLint dataType;

	// texture type
	GLenum textureType;

// dimensions
	// x, y, z, layers
	GLsizei width = 0;
	GLsizei height = 0;
	GLsizei depth = 1;
	GLsizei layers = 1;

// filtering
	GLint minFilter = GL_NEAREST;
	GLint magFilter = GL_NEAREST;

// wrap mode - they let you specifiy it different for different axes, but I don't ever use that
	GLint wrap = GL_CLAMP_TO_EDGE;

	// initial image data, for loading images
	void * initialData = nullptr;

	// data passed type, if relevant
	GLenum pixelDataType = GL_UNSIGNED_BYTE;
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
		texture_t tex;
		tex.creationOptions = texOptsIn;
		tex.textureUnit = GL_TEXTURE0 + count;
		tex.textureSize = texOptsIn.width * texOptsIn.height * texOptsIn.depth * texOptsIn.layers * bytesPerPixel( texOptsIn.dataType );

		// create the texture
		glGenTextures( 1, &tex.textureHandle );
		glActiveTexture( GL_TEXTURE0 + count );
		glBindTexture( texOptsIn.textureType, tex.textureHandle );
		switch ( texOptsIn.textureType ) {
		case GL_TEXTURE_2D:
		glTexImage2D( GL_TEXTURE_2D, 0, texOptsIn.dataType, texOptsIn.width, texOptsIn.height, 0, getFormat( texOptsIn.dataType ), texOptsIn.pixelDataType, texOptsIn.initialData );
		break;

		// todo
			// array textures
			// 3d textures

		default:
		break;
		}


		// add it to the list
		textures[ label ] = tex;

		// increment count on add, so we can use this for glActiveTexture( GL_TEXTURE0 + count )
		count++;
	}

	GLuint Get ( string label ) { // if the map contains the key, return it, else some nonsense value
		return ( textures.find( label ) != textures.end() ) ?
			textures[ label ].textureHandle : std::numeric_limits< GLuint >::max();
	}

	GLuint GetUnit ( string label ) {
		return ( textures.find( label ) != textures.end() ) ?
			textures[ label ].textureUnit : std::numeric_limits< GLuint >::max();
	}

	void EnumerateUnitUsage () {
		// give me a report for all the active textures like:
			// 0 : "Accumulator" ( unit : label ... type/dimensions information? )
	}

	// total size in bytes
	size_t TotalSize () {
		size_t total = 0;
		for ( auto& tex : textures )
			total += tex.second.textureSize;
		return total;
	}

	// TODO
	void Remove ( string label ) {
		// delete texture
	}

	// TODO
	void Shutdown ()  {
		// delete all textures
	}

	std::unordered_map< string, texture_t > textures;
};

#endif // TEXTURE_H
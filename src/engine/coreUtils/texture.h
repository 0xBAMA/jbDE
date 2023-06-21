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
	case GL_RGBA8UI:			return 4 * 1; break;
	case GL_RGBA16F:			return 4 * 2; break;
	default:
		cout << "unknown type texture created" << endl;
		return 0;
		break;
	}
}

inline GLenum getFormat( GLint internalFormat ) {
	switch ( internalFormat ) { // hitting the commonly used formats
	case GL_R8:
	case GL_R16:
	case GL_R16F:
	case GL_R32F:
		return GL_RED;
		break;

	case GL_R8I:
	case GL_R8UI:
	case GL_R16I:
	case GL_R16UI:
	case GL_R32I:
	case GL_R32UI:
		return GL_RED_INTEGER;
		break;

	case GL_RG8:
	case GL_RG16:
	case GL_RG16F:
	case GL_RG32F:
		return GL_RG;
		break;

	case GL_RG8I:
	case GL_RG8UI:
	case GL_RG16I:
	case GL_RG16UI:
	case GL_RG32I:
	case GL_RG32UI:
		return GL_RG_INTEGER;
		break;

	case GL_RGB4:
	case GL_RGB5:
	case GL_RGB8:
	case GL_RGB12:
	case GL_RGBA2:
	case GL_RGBA4:
	case GL_SRGB8:
	case GL_RGB16F:
	case GL_RGB32F:
	case GL_R11F_G11F_B10F:
	case GL_RGB9_E5:
		return GL_RGB;
		break;

	case GL_RGB8I:
	case GL_RGB8UI:
	case GL_RGB16I:
	case GL_RGB16UI:
	case GL_RGB32I:
	case GL_RGB32UI:
		return GL_RGB_INTEGER;
		break;

	case GL_RGBA8:
	case GL_RGBA12:
	case GL_RGBA16:
	case GL_RGBA16F:
	case GL_RGBA32F:
		return GL_RGBA;
		break;

	case GL_RGBA8I:
	case GL_RGBA8UI:
	case GL_RGBA16I:
	case GL_RGBA16UI:
	case GL_RGBA32I:
	case GL_RGBA32UI:
		return GL_RGBA_INTEGER;
		break;

	default:
		return 0;
		break;
	}
}

//===== Texture Options ===============================================================================================
struct textureOptions_t {

	// simplify this so that it's just handling the case of 2d textures - build from there

	// this will need to be simplified - just straight fill it out to match glTexImage2D()

		// need to also include filtering
		// need to also include wrap modes
		// ...

};


//===== Texture Record ================================================================================================
struct texture_t {

	// keep the information used to create the texture
	textureOptions_t creationOptions;

	GLuint textureHandle;
	GLenum textureUnit; // GL_TEXTURE0 + N
	size_t textureSize;

	string label;

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

	// todo: update the below ( Get/GetUnit ) from the unordered map version to the vector version

	GLuint Get ( string label ) { // if the map contains the key, return it, else some nonsense value
		// return ( textures.find( label ) != textures.end() ) ?
		// 	textures[ label ].textureHandle : std::numeric_limits< GLuint >::max();
	}

	GLuint GetUnit ( string label ) {
		// return ( textures.find( label ) != textures.end() ) ?
		// 	textures[ label ].textureUnit : std::numeric_limits< GLuint >::max();
	}

	void Update ( string label /*, ... */ ) {
		// pass in new data for the texture
	}

	void EnumerateUnitUsage () {
		// give me a report for all the active textures like:
			// 0 : "Accumulator" ( unit : label ... type/dimensions information? )

		cout << "Texture Unit Usage:" << endl;
		for ( auto& tex : textures ) {
			cout << "  " << tex.textureUnit << " : " << tex.label << endl;
		}
	}

	// total size in bytes
	size_t TotalSize () {
		size_t total = 0;
		for ( auto& tex : textures )
			total += tex.textureSize;
		return total;
	}

	// TODO
	void Remove ( string label ) {
		// delete texture
	}

	// TODO
	void Shutdown () {
		// delete all textures
	}

	std::vector< texture_t > textures; // keep as an ordered set
};

#endif // TEXTURE_H
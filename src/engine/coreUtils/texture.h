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
	case GL_RGBA8:
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
		// enum target
		// int level
		// int internalformat
		// size_t width
		// size_t height
		// int border
		// enum format
		// enum type ( of the passed data )
		// void pointer to data

		// need to also include filtering ( min + mag )
		// need to also include wrap modes
		// ...

};


//===== Texture Record ================================================================================================
struct texture_t {

	// keep the information used to create the texture
	textureOptions_t creationOptions;

	GLuint textureHandle;
	// GLenum textureUnit; // GL_TEXTURE0 + N ... this is not how to do it
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

		texture_t tex;
		tex.creationOptions = texOptsIn;
		// tex.textureUnit = count;

		// blah blah create the texture

		// store for later
		textures.push_back( tex );

		// increment count on add, so we can use this for glActiveTexture( GL_TEXTURE0 + count )
		count++;
	}

	// todo: update the below ( Get/GetUnit ) from the unordered map version to the vector version

	GLuint Get ( const string label ) { // if the map contains the key, return it, else some nonsense value
		for ( auto& tex : textures ) {
			if ( tex.label == label ) {
				return tex.textureHandle;
			}
		}
		return std::numeric_limits< GLuint >::max();
	}

	GLint GetType ( const string label ) {
		for ( auto& tex : textures ) {
			if ( tex.label == label ) {
				// return tex.creationOptions.internalformat; // this is the move
			}
		}
		return std::numeric_limits< GLint >::max();
	}

// I think this is the way we're going to use this now...
	// additionally, we can drop the glUniform1i if using layout qualifiers in the shader cod

	// so an example call is textureManager.BindTexForShader( "Display Texture", "current", shaders[ "Display" ], 0 );
	void BindTexForShader ( string label, const string shaderSampler, const GLuint shader, int location ) {
		glBindTextureUnit( location, Get( label ) );
		glUniform1i( glGetUniformLocation( shader, shaderSampler.c_str() ), location );
	}

	// so an example call is textureManager.BindImageForShader( "Display Texture", "current", shaders[ "Display" ], 0 );
	void BindImageForShader ( string label, const string shaderSampler, const GLuint shader, int location ) {
		glBindImageTexture( location, Get( label ), 0, GL_TRUE, 0, GL_READ_WRITE, GetType( label ) );
		glUniform1i( glGetUniformLocation( shader, shaderSampler.c_str() ), location );
	}

	void Update ( string label /*, ... */ ) {
		// pass in new data for the texture
	}

	void EnumerateTextures () {
		// give me a report for all the active textures like:
			// 0 : "Accumulator" ( unit : label ... type/dimensions information? )

		cout << "Textures :" << endl;
		for ( auto& tex : textures ) {
			cout << "  " << tex.textureHandle << " : " << tex.label << " ( " << tex.textureSize << " )" << endl;
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
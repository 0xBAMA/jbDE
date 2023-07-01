#pragma once
#ifndef TEXTURE_H
#define TEXTURE_H

#include "../includes.h"

//===== Helper Functions ==============================================================================================
inline size_t bytesPerPixel ( GLint type ) {
	// over time we'll accumulate all the ones that I use, this currently is sufficient
	switch ( type ) {
	// depth formats
	case GL_DEPTH_COMPONENT32:	return 1 * 4; break;
	// two channel formats
	case GL_RG32UI:				return 2 * 4; break;
	// three channel formats
	case GL_RGB8:				return 3 * 1; break;
	// four channel formats
	case GL_RGBA8:
	case GL_RGBA8UI:			return 4 * 1; break;
	case GL_RGBA16F:			return 4 * 2; break;
	case GL_RGBA32F:			return 4 * 4; break;
	default:
		cout << "unknown type texture created" << endl;
		return 0;
		break;
	}
}

inline GLenum getFormat ( GLint internalFormat ) {
	switch ( internalFormat ) { // hitting the commonly used formats

	// depth formats
	case GL_DEPTH_COMPONENT:
	case GL_DEPTH_COMPONENT16:
	case GL_DEPTH_COMPONENT24:
	case GL_DEPTH_COMPONENT32:
		return GL_DEPTH_COMPONENT;
		break;

	// one-channel formats
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

	// two-channel formats
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

	// three-channel formats
	case GL_RGB4:
	case GL_RGB5:
	case GL_RGB8:
	case GL_RGB12:
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

	// four-channel formats
	case GL_RGBA2:
	case GL_RGBA4:
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
// data required to create a texture
struct textureOptions_t {
	// data type
	GLint dataType;

	// texture type
	GLenum textureType;

// dimensions
	// x, y, z, layers
	GLsizei width = 0;
	GLsizei height = 0;
	GLsizei depth = 1;	// will become relevant with 3d textures
	GLsizei layers = 1;	// not technically correctly handled right now, ignores value

// filtering - default to nearest filtering
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
	string label;			// identifier for the texture

	// keep the information used to create the texture
	textureOptions_t creationOptions;

	GLuint textureHandle;	// from glGenTextures()
	size_t textureSize;		// number of bytes on disk
};

//===== Texture Manager ===============================================================================================
class textureManager_t {
public:
	// prevent the use of 4.5 features
	const bool compatibilityMode = false;
	const bool statsReport = false;

	void Init () {

		if ( statsReport ) {

			//report some platform details:
			//	number of available texture units
			//	maximum dimensions of texture
			//	...

			GLint val;
			glGetIntegerv( GL_MAX_TEXTURE_SIZE, &val );
			cout << endl << endl << "\t\tMax Texture Size Reports: " << val << endl;

			glGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &val );
			cout << "\t\tMax 3D Texture Size Reports: " << val << endl;

			glGetIntegerv( GL_MAX_DRAW_BUFFERS, &val );
			cout << "\t\tMax Draw Buffers: " << val << endl;

			glGetIntegerv( GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, &val );
			cout << "\t\tMax Compute Texture Image Units Reports: " << val << endl;

			glGetIntegerv( GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &val );
			cout << "\t\tMax Combined Texture Image Units: " << val << endl;

			glGetIntegerv( GL_MAX_VIEWPORT_DIMS, &val );
			cout << "\t\tMax Viewport Dims: " << val << endl << endl;

			// make up for the spacing issues
			cout << "\t............................................................. ";

		}

		// any additional init? not sure

	}

	int count = 0;
	void Add ( string label, textureOptions_t &texOptsIn ) {

		texture_t tex;
		tex.label = label;
		tex.creationOptions = texOptsIn;
		tex.textureSize = texOptsIn.width * texOptsIn.height * texOptsIn.depth * texOptsIn.layers * bytesPerPixel( texOptsIn.dataType );

		const bool needsMipmap = ( // does this texture need to generate mipmaps
			texOptsIn.minFilter == GL_NEAREST_MIPMAP_NEAREST ||
			texOptsIn.minFilter == GL_LINEAR_MIPMAP_NEAREST ||
			texOptsIn.minFilter == GL_NEAREST_MIPMAP_LINEAR ||
			texOptsIn.minFilter == GL_LINEAR_MIPMAP_LINEAR
		);

		// generate the texture
		glGenTextures( 1, &tex.textureHandle );
		glBindTexture( texOptsIn.textureType, tex.textureHandle );
		switch ( texOptsIn.textureType ) {
		case GL_TEXTURE_2D:
			glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, texOptsIn.minFilter );
			glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, texOptsIn.magFilter );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, texOptsIn.wrap );
			glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, texOptsIn.wrap );
			glTexImage2D( GL_TEXTURE_2D, 0, texOptsIn.dataType, texOptsIn.width, texOptsIn.height, 0, getFormat( texOptsIn.dataType ), texOptsIn.pixelDataType, texOptsIn.initialData );
			if ( needsMipmap == true ) {
				glGenerateMipmap( GL_TEXTURE_2D );
			}
			break;

		case GL_TEXTURE_2D_ARRAY:
			// todo - required for ProjectedFramebuffers ( rendering sponza, again, but much cooler this time )
			break;

		case GL_TEXTURE_3D:
			// todo - required for Voraldo
			break;

		default:
		break;
		}

		// store for later
		textures.push_back( tex );

		// increment count on add - we'll keep it for the time being
		count++; // kind of redundant with textures.size(), tbd
	}

	GLuint Get ( const string label ) { // if the array contains the key, return it, else some nonsense value
		for ( auto& tex : textures ) {
			if ( tex.label == label ) {
				return tex.textureHandle;
			}
		}
		cout << "Texture Missing" << endl;
		return std::numeric_limits< GLuint >::max();
	}

	GLint GetType ( const string label ) {
		for ( auto& tex : textures ) {
			if ( tex.label == label ) {
				return tex.creationOptions.dataType; // this is the move
			}
		}
		return std::numeric_limits< GLint >::max();
	}

// I think this is the way we're going to use this now...
	// additionally, we can drop the glUniform1i if using layout qualifiers in the shader code

	// so an example call is textureManager.BindTexForShader( "Display Texture", "current", shaders[ "Display" ], 0 );
	void BindTexForShader ( string label, const string shaderSampler, const GLuint shader, int location ) {
		// if 4.5 feature set is not supported
		if ( compatibilityMode == true ) {
			glActiveTexture( GL_TEXTURE0 + location );
			glBindTexture( GL_TEXTURE_2D, Get( label ) );
		} else {
			glBindTextureUnit( location, Get( label ) );
		}
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

		int maxWidth = 0; // maximum width of the bytes value, across all textures
		for ( auto& tex : textures ) {
			maxWidth = std::max( maxWidth, int( GetWithThousandsSeparator( tex.textureSize ).length() ) );
		}

		const int reportWidth = 48;
		cout << "  Textures :" << endl;
		for ( auto& tex : textures ) {
			std::stringstream s;
			s << "    " << std::setw( 3 ) << std::setfill( '0' ) << tex.textureHandle << " : " << tex.label << " ";
			cout << s.str();
			for ( unsigned int i = 0; i < reportWidth - s.str().size(); i++ ) {
				cout << ".";
			}
			cout << " ( " << std::setw( maxWidth ) << std::setfill( ' ' ) << GetWithThousandsSeparator( tex.textureSize ) << " bytes )" << endl;
		}
		cout << endl;
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

// move bindsets to here
	// bindsets get a new field to tell texture/image bind type

#endif // TEXTURE_H
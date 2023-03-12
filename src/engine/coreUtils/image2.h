#pragma once
#ifndef IMAGE2_H
#define IMAGE2_H

//===== LodePNG =======================================================================================================
// Lode Vandevenne's LodePNG PNG Load/Save lib
#include "../../imageLibs/LodePNG/lodepng.h"		// https://github.com/lvandeve/lodepng

//===== STB ===========================================================================================================
// Sean Barrett's public domain load, save, resize libs - need corresponding define in the ./stb/impl.cc file,
	// before their inclusion, which is done by the time compilation hits this point - they can be straight
	// included, here, as follows:
#include "../../imageLibs/stb/stb_image.h"			// https://github.com/nothings/stb/blob/master/stb_image.h
#include "../../imageLibs/stb/stb_image_write.h"	// https://github.com/nothings/stb/blob/master/stb_image_write.h
#include "../../imageLibs/stb/stb_image_resize.h"	// https://github.com/nothings/stb/blob/master/stb_image_resize.h

//===== tinyEXR =======================================================================================================
// TinyEXR is for loading and saving of high bit depth images - 16, 32 bits, etc
#include "../../imageLibs/tinyEXR/tinyexr.h"		// https://github.com/syoyo/tinyexr

//===== STL ===========================================================================================================
#include <array>
#include <vector>
#include <random>
#include <string>
#include <iostream>
#include <type_traits> // for std::is_same - https://en.cppreference.com/w/cpp/types/is_same

//===== Image2 ========================================================================================================

template < typename imageType, int numChannels > class Image2 {
public:

	enum class backend { STB_IMG, LODEPNG, TINYEXR };

	struct color {
		std::array< imageType, numChannels > data;
	};

//===== Constructors ==================================================================================================

	// init emtpy image with given dimensions
	Image2 ( uint32_t x, uint32_t y ) : width( x ), height( y ) {
		data.resize( width * height * numChannels, 0 );
	}

	// load image from path
	Image2 ( string path, backend loader = backend::LODEPNG ) {
		if ( !Load( path, loader ) ) {
			cout << "image load failed with path " << path << newline << flush;
		}
	}

	// load from e.g. GPU memory
	Image2 ( uint32_t x, uint32_t y, imageType* contents ) : width( x ), height( y ) {
		const size_t numBers = width * height * numChannels;
		data.reserve( numBers );
		for ( size_t idx = 0; idx < numBers; idx++ ) {
			data.push_back( idx );
		}
	}

//===== Functions =====================================================================================================

// access to private data
	// get color at xy
	// set color at xy
	// get width
	// get height

// more esoteric stuff
	// swizzle ( reorganize channels, like irFlip2 )
	// pixel sorting - need to figure out thresholding
	// dithering, CPU side, could be of value
	// get average color value across all pixels in the image
	// crop image

//===== Basic =========================================================================================================

	bool Load ( string path, backend loader = backend::LODEPNG ) {
		switch ( loader ) {
			case backend::STB_IMG: return LoadSTB_img( path ); break;
			case backend::LODEPNG: return LoadLodePNG( path ); break;
			case backend::TINYEXR: return LoadTinyEXR( path ); break;
			default: return false;
		}
	}

	bool Save ( string path, backend loader = backend::LODEPNG ) {
		switch ( loader ) {
			case backend::STB_IMG: return SaveSTB_img( path ); break;
			case backend::LODEPNG: return SaveLodePNG( path ); break;
			case backend::TINYEXR: return SaveTinyEXR( path ); break;
			default: return false;
		}
	}

	void FlipHorizontal () {
		// back up the existing state of the image
		std::vector< imageType > oldData;
		oldData.reserve( width * height * numChannels );
		for ( auto c : data ) oldData.push_back( c );
		data.resize( 0 );
		data.reserve( width * height * 4 );
		for ( uint32_t y = 0; y < height; y++ ) {
			for ( uint32_t x = 0; x < width; x++ ) {
				for ( uint8_t c = 0; c < numChannels; c++ ) {
					data.push_back( oldData[ ( ( width - x - 1 ) + y * width ) * numChannels + c ] );
				}
			}
		}
	}

	void FlipVertical () {
		// back up the existing state of the image
		std::vector< imageType > oldData;
		oldData.reserve( width * height * numChannels );
		for ( auto c : data ) oldData.push_back( c );
		data.resize( 0 );
		data.reserve( width * height * numChannels );
		for ( uint32_t y = 0; y < height; y++ ) {
			for ( uint32_t x = 0; x < width; x++ ) {
				for ( uint8_t c = 0; c < numChannels; c++ ) {
					data.push_back( oldData[ ( x + ( height - y - 1 ) * width ) * numChannels + c ] );
				}
			}
		}
	}

	void Resize ( float factor ) {
		int newX = std::floor( factor * float( width ) );
		int newY = std::floor( factor * float( height ) );

		// create a copy of the existing data
		const uint32_t oldSize = width * height * numChannels;
		imageType* oldData = ( imageType* ) malloc( oldSize * sizeof( imageType ) );
		for ( uint32_t i = 0; i < oldSize; i++ ) {
			oldData[ i ] = data[ i ];
		}

		// buffer needs to be allocated before the resize function runs
		imageType* newData = ( imageType* ) malloc( newX * newY * numChannels * sizeof( imageType ) );

		// compiler is complaining without the casts, I have no idea why that would be neccesary - it should have the correct type from imageType
			// I guess the is_same<> does not run during template instantiation? hard to say, I don't really have any insight here
		if ( std::is_same< uint8_t, imageType >::value ) { // uint type
			stbir_resize_uint8( ( const uint8_t* ) oldData, width, height, width * numChannels * sizeof( imageType ), ( uint8_t* ) newData, newX, newY, newX * numChannels * sizeof( imageType ), numChannels );
		} else { // float type
			stbir_resize_float( ( const float* ) oldData, width, height, width * numChannels * sizeof( imageType ), ( float* ) newData, newX, newY, newX * numChannels * sizeof( imageType ), numChannels );
		}

		// image dimensions are now scaled up by the input scale factor
		width = newX;
		height = newY;

		// copy new data back to the data vector
		data.resize( 0 );
		const uint32_t newSize = width * height * numChannels;
		data.reserve( newSize );
		for ( uint32_t i = 0; i < newSize; i++ ) {
			data.push_back( newData[ i ] );
		}
	}

	// color GetColorAtXY ( uint32_t x, uint32_t y ) {

	// }

	// void SetColorAtXY ( uint32_t x, uint32_t y, color set ) {

	// }

private:
//===== Internal Data =================================================================================================

	// image dimensions ( make public or add accessors? )
	uint32_t width = 0;
	uint32_t height = 0;

	// image data
	std::vector< imageType > data;

//===== Loader Functions == ( Accessed via Load() ) ===================================================================

	// this will handle a number of extensions
	bool LoadSTB_img ( string path ) {
		std::vector< uint8_t > loadedData;
		int w,h,n=w=h=0;
		unsigned char *image = stbi_load( path.c_str(), &w, &h, &n, STBI_rgb_alpha );
		data.resize( 0 );
		width = ( uint32_t ) w;
		height = ( uint32_t ) h;
		const uint32_t numPixels = width * height;
		data.reserve( numPixels * numChannels );
		const bool uintType = std::is_same< uint8_t, imageType >::value;
		for ( uint32_t i = 0; i < numPixels; i++ ) {
			data.push_back( uintType ? image[ i * numChannels + 0 ] : image[ i * numChannels + 0 ] / 255.0f );
			data.push_back( uintType ? image[ i * numChannels + 1 ] : image[ i * numChannels + 1 ] / 255.0f );
			data.push_back( uintType ? image[ i * numChannels + 2 ] : image[ i * numChannels + 2 ] / 255.0f );
			data.push_back( n == 4 ? ( uintType ? image[ i * numChannels + 3 ] : image[ i * numChannels + 3 ] / 255.0f ) : uintType ? 255 : 1.0f );
		}
		return true;
	}

	// preferred loader for .png
	bool LoadLodePNG ( string path ) {
		std::vector< uint8_t > loaded;
		unsigned error = lodepng::decode( loaded, width, height, path.c_str() );
		if ( !error ) {
			const size_t num = width * height * numChannels;
			data.reserve( num );
			for ( size_t idx = 0; idx < num; idx++ ) {
				// uint image type, use data directly, float needs to be remapped to [0.0, 1.0]
				data.push_back( std::is_same< uint8_t, imageType >::value ? loaded[ idx ] : loaded[ idx ] / 255.0f );
			}
			return true;
		} else {
			cout << "lodepng load error: " << lodepng_error_text( error ) << endl;
			return false;
		}
	}

	// high bit depth images, .exr format
	bool LoadTinyEXR ( string path ) {
		float* out; // width * height * RGBA
		const char* errorCode = NULL;
		int w, h;
		int ret = LoadEXR( &out, &w, &h, path.c_str(), &errorCode );
		if ( ret != TINYEXR_SUCCESS ) {
			if ( errorCode ) {
				fprintf( stderr, "ERR : %s\n", errorCode );
				FreeEXRErrorMessage( errorCode ); // release memory of error message.
			}
		} else {
			width = w; height = h;
			data.resize( width * height * 4 );
			const bool uintType = std::is_same< uint8_t, imageType >::value;
			for ( size_t idx = 0; idx < width * height * numChannels; idx++ ) {
				// copy to the image data
				data[ idx ] = uintType ? out[ idx ] * 255.0f : out[ idx ];
			}
			free( out ); // release memory of image data
		}
		return ret;
	}

//===== Save Functions == ( Accessed via Save() ) =====================================================================

	bool SaveSTB_img ( string path ) {
		// TODO: figure out return value semantics for error reporting, it's an int, I didn't read the header very closely
		if ( std::is_same< uint8_t, imageType >::value ) {
			return stbi_write_png( path.c_str(), width, height, 8, &data[ 0 ], width * numChannels );
		} else { // float type
			std::vector< uint8_t > remappedData;
			remappedData.reserve( data.size() );
			for ( auto& val : data ) {
				remappedData.push_back( val * 255.0f );
			}
			return stbi_write_png( path.c_str(), width, height, 8, &remappedData[ 0 ], width * numChannels );
		}
	}

	bool SaveLodePNG ( string path ) {
		const bool uintType = std::is_same< uint8_t, imageType >::value;
		if ( uintType ) {
			unsigned error = lodepng::encode( path.c_str(), ( uint8_t* ) data.data(), width, height );
			if ( !error ) {
				return true;
			} else {
				std::cout << "lodepng save error: " << lodepng_error_text( error ) << std::endl;
				return false;
			}
		} else {
			// remap the float data to uints before saving
			std::vector< uint8_t > remappedData;
			remappedData.reserve( data.size() );
			for ( auto& val : data ) {
				remappedData.push_back( std::clamp( val * 255.0f, 0.0f, 255.0f ) );
			}
			unsigned error = lodepng::encode( path.c_str(), ( uint8_t* ) remappedData.data(), width, height );
			if ( !error ) {
				return true;
			} else {
				std::cout << "lodepng save error: " << lodepng_error_text( error ) << std::endl;
				return false;
			}
		}
	}

	bool SaveTinyEXR ( string path ) {
		EXRHeader header;
		EXRImage image;
		InitEXRHeader( &header );
		InitEXRImage( &image );

		// maybe at some point rethink this - this kind of assumes 4-channel float
		image.num_channels = 4;

		std::vector<float> images[ 4 ];
		images[ 0 ].resize( width * height );
		images[ 1 ].resize( width * height );
		images[ 2 ].resize( width * height );
		images[ 3 ].resize( width * height );

		// Split RGBARGBARGBA... into R, G, B, A layers
		for ( size_t i = 0; i < width * height; i++ ) {
			images[ 0 ][ i ] = data[ 4 * i + 0 ];
			images[ 1 ][ i ] = data[ 4 * i + 1 ];
			images[ 2 ][ i ] = data[ 4 * i + 2 ];
			images[ 3 ][ i ] = data[ 4 * i + 3 ];
		}

		float* image_ptr[ 4 ];
		image_ptr[ 0 ] = &( images[ 3 ].at( 0 ) ); // A
		image_ptr[ 1 ] = &( images[ 2 ].at( 0 ) ); // B
		image_ptr[ 2 ] = &( images[ 1 ].at( 0 ) ); // G
		image_ptr[ 3 ] = &( images[ 0 ].at( 0 ) ); // R

		image.images = ( unsigned char** ) image_ptr;
		image.width = width;
		image.height = height;

		header.num_channels = 4;
		header.channels = ( EXRChannelInfo * ) malloc( sizeof( EXRChannelInfo ) * header.num_channels );
		// Must be (A)BGR order, since most of EXR viewers expect this channel order.
		strncpy( header.channels[ 0 ].name, "A", 255 ); header.channels[ 0 ].name[ strlen( "A" ) ] = '\0';
		strncpy( header.channels[ 1 ].name, "B", 255 ); header.channels[ 1 ].name[ strlen( "B" ) ] = '\0';
		strncpy( header.channels[ 2 ].name, "G", 255 ); header.channels[ 2 ].name[ strlen( "G" ) ] = '\0';
		strncpy( header.channels[ 3 ].name, "R", 255 ); header.channels[ 3 ].name[ strlen( "R" ) ] = '\0';

		header.pixel_types = ( int * ) malloc( sizeof( int ) * header.num_channels );
		header.requested_pixel_types = ( int * ) malloc( sizeof( int ) * header.num_channels );
		for ( int i = 0; i < header.num_channels; i++ ) {
			header.pixel_types[ i ] = TINYEXR_PIXELTYPE_FLOAT; // pixel type of input image
			// header.requested_pixel_types[ i ] = TINYEXR_PIXELTYPE_HALF; // pixel type of output image to be stored in .EXR
			header.requested_pixel_types[ i ] = TINYEXR_PIXELTYPE_FLOAT;
		}

		const char* err = NULL; // or nullptr in C++11 or later.
		int ret = SaveEXRImageToFile( &image, &header, path.c_str(), &err );
		if ( ret != TINYEXR_SUCCESS ) {
			fprintf( stderr, "Save EXR err: %s\n", err );
			FreeEXRErrorMessage( err ); // free's buffer for an error message
		}

		// free( rgb );
		free( header.channels );
		free( header.pixel_types );
		free( header.requested_pixel_types );
		return ret;
	}

};

//===== Common Usage Aliases ==========================================================================================
// standard 8bpc color + alpha
typedef Image2< uint8_t, 4 > Image_4U;
typedef Image2< uint8_t, 4 >::color color_4U;

// 1 channel float e.g. Depth
typedef Image2< float, 1 > Image_1F;
typedef Image2< float, 1 >::color color_1F;

// 4 channel float e.g. HDR color
typedef Image2< float, 4 > Image_4F;
typedef Image2< float, 4 >::color color_4F;


#endif // IMAGE2_H
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
#include <type_traits> // for std::is_same

//===== Image2 ========================================================================================================

template < typename imageType, int numChannels > class Image2 {
public:
	enum class backend { STB, LODEPNG };
	struct color {
		std::array< imageType, numChannels > data;
	};

//===== Constructors ==================================================================================================

	// init emtpy image with given dimensions
	Image2 ( uint32_t x, uint32_t y ) : width( x ), height( y ) {
		data.resize( width * height * numChannels, 0 );
	}

	// load image from path
	Image2 ( std::string path, backend loader = LODEPNG ) {
	
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

// basic
	// load image
	// save image, additionally, support for EXR format
	// flip horizontal
	// flip vertical
	// resize image
	// randomize contents
	// swizzle ( reorganize channels, like irFlip2 )
	// reset color to some value

// access to private data
	// get color at xy
	// set color at xy

// more stuff
	// pixel sorting - need to figure out thresholding
	// dithering, CPU side, could be of value
	// blend image? figure out what this should look like
	// crop image
	// get average color value for the image


private:
	// image dimensions
	uint32_t width = 0;
	uint32_t height = 0;

	// image data
	std::vector< imageType > data;
};

//===== Aliases =======================================================================================================
// standard 8bpc color+alpha
typedef Image2< uint8_t, 4 > Image2_4u;
typedef Image2< uint8_t, 4 >::color color_4u;

// 1 channel float
typedef Image2< float, 1 > Image2_1f;
typedef Image2< float, 1 >::color color_1f;

// 4 channel float
typedef Image2< float, 4 >::color color_4f;
typedef Image2< float, 4 > Image2_4f;


#endif // IMAGE2_H
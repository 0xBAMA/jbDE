#pragma once
#ifndef PALETTE_H
#define PALETTE_H

#include "../GLM/glm.hpp"
#include "../engine/coreUtils/image.h"

struct palette {
	string label;
	std::vector< glm::ivec3 > colors;
};

static std::vector< palette > paletteList;
static void LoadPalettes () {
	Image paletteRecord( "./src/data/palettes.png" );
	for ( uint32_t yPos = 0; yPos < paletteRecord.height; yPos++ ) {
		palette p;

		// first 32 pixels' red channels contain the space-padded label
		for ( int x = 0; x < 32; x++ ) { p.label += char( paletteRecord.GetAtXY( x, yPos ).r ); }
		p.label.erase( std::remove( p.label.begin(), p.label.end(), ' ' ), p.label.end() );

		// then the rest of the row, up to the first { 0, 0, 0, 0 } pixel is the palette data
		for ( int x = 33;; x++ ) {
			rgba read = paletteRecord.GetAtXY( x, yPos );
			if ( read.a == 0 ) { break; }
			p.colors.push_back( glm::ivec3( read.r, read.g, read.b ) );
		}

		// data is ready to go
		paletteList.push_back( p );
	}
}

#endif // PALETTE_H
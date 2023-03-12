#pragma once
#ifndef WORDLIST
#define WORDLIST

#include <string>
#include <vector>

#include "../engine/coreUtils/image2.h"

static std::vector< std::string > badWords;
static inline void LoadBadWords () {
	Image_4U source( "./src/data/wordlistBad.png" );
	for ( uint32_t yPos = 0; yPos < source.Height(); yPos++ ) {
		string s;
		for ( uint32_t x = 0; x < source.Width(); x++ ) { s += char( source.GetAtXY( x, yPos )[ red ] ); }
		s.erase( std::remove( s.begin(), s.end(), ' ' ), s.end() );
		badWords.push_back( s );
	}
}

static std::vector< std::string > colorWords;
static inline void LoadColorWords () {
	Image_4U source( "./src/data/wordlistColor.png" );
	for ( uint32_t yPos = 0; yPos < source.Height(); yPos++ ) {
		string s;
		for ( uint32_t x = 0; x < source.Width(); x++ ) { s += char( source.GetAtXY( x, yPos )[ red ] ); }
		s.erase( std::remove( s.begin(), s.end(), ' ' ), s.end() );
		colorWords.push_back( s );
	}
}

// TURQUOISE
// EMERALD
// PETER RIVER
// AMETHYST
// WET ASPHALT
// GREEN SEA
// NEPHRITIS
// BELIZE HOLE
// WISTERIA
// MIDNIGHT BLUE
// SUN FLOWER
// CARROT
// ALIZARIN
// CLOUDS
// CONCRETE
// ORANGE
// PUMPKIN
// POMEGRANATE
// SILVER
// ASBESTOS

#endif
#pragma once
#ifndef WORDLIST
#define WORDLIST

#include <string>
#include <vector>

#include "../engine/coreUtils/image.h"

static std::vector< std::string > badWords;
static inline void LoadBadWords () {
	Image source( "./src/data/wordlistBad.png" );
	for ( uint32_t yPos = 0; yPos < source.height; yPos++ ) {
		string s;
		for ( uint32_t x = 0; x < source.width; x++ ) { s += char( source.GetAtXY( x, yPos ).r ); }
		s.erase( std::remove( s.begin(), s.end(), ' ' ), s.end() );
		badWords.push_back( s );
	}
}

static std::vector< std::string > colorWords;
static inline void LoadColorWords () {
	Image source( "./src/data/wordlistColor.png" );
	for ( uint32_t yPos = 0; yPos < source.height; yPos++ ) {
		string s;
		for ( uint32_t x = 0; x < source.width; x++ ) { s += char( source.GetAtXY( x, yPos ).r ); }
		s.erase( std::remove( s.begin(), s.end(), ' ' ), s.end() );
		colorWords.push_back( s );
	}
}

#endif
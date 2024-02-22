#include "../../../engine/includes.h"

#pragma once
#ifndef TILEDISPENSER_H
#define TILEDISPENSER_H

class tileDispenser {
public:
	uint32_t tileSize;
	uint32_t imageWidth;
	uint32_t imageHeight;

	std::vector< ivec2 > tiles;
	uint32_t tileListOffset;
	uint32_t tileListPasses;

	tileDispenser( uint32_t t, uint32_t w, uint32_t h ) :
		tileSize( t ), imageWidth( w ), imageHeight( h ) {
		RegenerateTileList();
	}

	// regenerate list
	void RegenerateTileList() {
		tiles.resize( 0 );
		tileListOffset = 0;
		tileListPasses = 0;
		for ( uint32_t x = 0; x < imageWidth; x += tileSize ) {
			for ( uint32_t y = 0; y < imageHeight; y += tileSize ) {
				tiles.push_back( ivec2( x, y ) );
			}
		}
		// shuffle the generated array
		std::random_device rd; std::mt19937 rngen( rd() );
		std::shuffle( tiles.begin(), tiles.end(), rngen );
	}

	// reset state
	void Reset( uint32_t t, uint32_t w, uint32_t h ) {
		tileSize = t;
		imageWidth = w;
		imageHeight = h;
		RegenerateTileList();
	}

	// get a tile from the list
	ivec2 GetTile() {
		// catch the case where we are completed a fullscreen pass
		if ( ++tileListOffset == tiles.size() ) {
			tileListOffset = 0;
			tileListPasses++;
		}
		return tiles[ tileListOffset ];
	}
};

#endif
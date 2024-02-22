#include "tileDispenser.h"

struct daedalusConfig_t {

	daedalusConfig_t() {
		// configuring the accumulator
		targetWidth = 640;
		targetHeight = 360;
		tileSize = 16;

		// initialize the tile dispenser
		tiles = tileDispenser( tileSize, targetWidth, targetHeight );

		// intialize the main view
		showConfigWindow = true;
		outputZoom = 1.0f;
		outputOffset = vec2( 0.0f );
		dragBufferAmount = 2000.0f;

		// load a config file and shit?
			// tbd, that could be a nice way to handle this
	}

	// sizing for accumulators, etc
	uint32_t targetWidth;
	uint32_t targetHeight;
	uint32_t tileSize;
	tileDispenser tiles;

	// tabbed interface for configuring scene/rendering parameters
	bool showConfigWindow;

	// main display, grid pan + zoom
	float outputZoom;
	vec2 outputOffset;
	float dragBufferAmount;

};
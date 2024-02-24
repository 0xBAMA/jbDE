#include "tileDispenser.h"

struct rngen {
	rngi wangSeeder = rngi( 0, 10000000 );
	rngi blueNoiseOffset = rngi( 0, 512 );
};

struct daedalusConfig_t {

	daedalusConfig_t() {
		// configuring the accumulator
		targetWidth = 1920;
		targetHeight = 1080;
		tileSize = 16;

		// initialize the tile dispenser
		tiles = tileDispenser( tileSize, targetWidth, targetHeight );

		// setup performance monitor
		performanceHistorySamples = 250;
		timeHistory.resize( performanceHistorySamples );

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

// move this to a view manager class
	// tabbed interface for configuring scene/rendering parameters
	bool showConfigWindow;

	// performance settings / monitoring
	uint32_t performanceHistorySamples;
	std::deque< float > timeHistory;	// ms per frame

	// main display, grid pan + zoom
	float outputZoom;
	vec2 outputOffset;
	float dragBufferAmount;

	// class holding the random number generators
	rngen rng;
};
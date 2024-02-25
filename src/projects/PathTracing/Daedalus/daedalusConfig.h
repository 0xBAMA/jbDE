#include "tileDispenser.h"

struct rngen_t {
	rngi wangSeeder = rngi( 0, 10000000 );
	rngi blueNoiseOffset = rngi( 0, 512 );
};

struct viewConfig_t {
	bool edgeLines = true;
	bool centerLines = true;
	bool ROTLines = true;
	bool goldenLines = true;
	bool vignette = true;

	// main display, grid pan + zoom
	float outputZoom;
	vec2 outputOffset;
};

struct daedalusConfig_t {

	daedalusConfig_t() {
		// this will need to come in from elsewhere, eventually - for now, hardcoded screen res
		outputWidth = 2560;
		outputHeight = 1440;

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
		view.outputZoom = 1.0f;
		view.outputOffset = ( vec2( targetWidth, targetHeight ) - vec2( outputWidth, outputHeight ) ) / 2.0f;

		subpixelJitterMethod = 1;
		cameraMode = 0;

		// load a config file and shit? ( src/projects/PathTracing/Daedalus/config.json individual config )
			// tbd, that could be a nice way to handle this
	}

	// size of output
	uint32_t outputWidth;
	uint32_t outputHeight;

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

	// this needs to be nicer
	viewConfig_t view;

	int subpixelJitterMethod;
	int cameraMode;

	// class holding the random number generators
	rngen_t rng;
};
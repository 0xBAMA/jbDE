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

struct renderConfig_t {
	int subpixelJitterMethod;
	float uvScalar;
	float exposure;
	float FoV;

	vec3 viewerPosition;
	vec3 basisX;
	vec3 basisY;
	vec3 basisZ;

	bool thinLensEnable;
	float thinLensFocusDistance;
	float thinLensJitterRadius;
	int bokehMode;
	int cameraType;
};

struct daedalusConfig_t {

	daedalusConfig_t() {

		// ===============================

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

		// ===============================

		// intialize the main view
		showConfigWindow = true;
		view.outputZoom = 1.0f;
		view.outputOffset = ( vec2( targetWidth, targetHeight ) - vec2( outputWidth, outputHeight ) ) / 2.0f;

		// ===============================

		render.subpixelJitterMethod = 1;
		render.uvScalar = 1.0f;
		render.exposure = 1.0f;
		render.FoV = 0.618f;

		render.viewerPosition = vec3( 0.0f );
		render.basisX = vec3( 1.0f, 0.0f, 0.0f );
		render.basisY = vec3( 0.0f, 1.0f, 0.0f );
		render.basisZ = vec3( 0.0f, 0.0f, 1.0f );

		render.thinLensEnable = false;
		render.thinLensFocusDistance = 10.0f;
		render.thinLensJitterRadius = 1.0f;
		render.bokehMode = 0;
		render.cameraType = 0;

		// ===============================

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

	// performance settings / monitoring
	uint32_t performanceHistorySamples;
	std::deque< float > timeHistory;	// ms per frame

	bool showConfigWindow;	// tabbed interface for configuring scene/rendering parameters
	viewConfig_t view;		// parameters for ouput presentation
	renderConfig_t render;	// parameters for the tiled renderer

	// class holding the random number generators
	rngen_t rng;
};
#include "tileDispenser.h"

struct rngen_t {
	rngi wangSeeder = rngi( 0, 10000000 );
	rngi blueNoiseOffset = rngi( 0, 512 );
};

struct viewConfig_t {
	bool edgeLines = true;
	vec3 edgeLinesColor = vec3( 0.0618f );
	bool centerLines = true;
	vec3 centerLinesColor = vec3( 0.1618f, 0.0618f, 0.0f );
	bool ROTLines = true;
	vec3 ROTLinesColor = vec3( 0.0f, 0.0618f, 0.0618f );
	bool goldenLines = true;
	vec3 goldenLinesColor = vec3( 0.1618f, 0.0f, 0.0f );
	bool vignette = true;

	// main display, grid pan + zoom
	float outputZoom;
	vec2 outputOffset;
};

struct sceneConfig_t {
	// raymarch intersection config
	bool raymarchEnable;
	float raymarchMaxDistance;
	float raymarchUnderstep;
	int raymarchMaxSteps;

	// Hybrid DDA traversal
	bool ddaSpheresEnable;
	float ddaSpheresBoundSize;
	int ddaSpheresResolution;
	// mode config? may require a lot of parameterization...

	bool maskedPlaneEnable;

	// sky config
	uvec2 skyDims;
	bool skyNeedsUpdate;
	int skyMode;
	vec3 skyConstantColor1;
	vec3 skyConstantColor2;
	float skyTime;
	bool skyInvert;
	float skyBrightnessScalar;
};

struct renderConfig_t {
	int subpixelJitterMethod;
	float uvScalar;
	float exposure;
	float FoV;
	float epsilon;
	float maxDistance;

	vec3 viewerPosition;
	vec3 basisX;
	vec3 basisY;
	vec3 basisZ;

	bool thinLensEnable;
	float thinLensFocusDistance;
	float thinLensJitterRadius;
	int bokehMode;
	int cameraType;
	bool cameraOriginJitter;
	int maxBounces;

	sceneConfig_t scene;
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
		filterEveryFrame = false;
		filterSelector = 0;

		// ===============================

		render.subpixelJitterMethod = 1;
		render.uvScalar = 1.0f;
		render.exposure = 1.0f;
		render.FoV = 0.618f;
		render.epsilon = 0.001f;
		render.maxDistance = 100.0f;

		render.viewerPosition = vec3( 0.0f );
		render.basisX = vec3( 1.0f, 0.0f, 0.0f );
		render.basisY = vec3( 0.0f, 1.0f, 0.0f );
		render.basisZ = vec3( 0.0f, 0.0f, 1.0f );

		render.thinLensEnable = false;
		render.thinLensFocusDistance = 10.0f;
		render.thinLensJitterRadius = 1.0f;
		render.bokehMode = 4;
		render.cameraType = 0;
		render.cameraOriginJitter = true;
		render.maxBounces = 10;

		// render.scene.raymarchEnable = true;
		render.scene.raymarchEnable = false;
		render.scene.raymarchMaxDistance = render.maxDistance; // do I want to keep both? not sure
		render.scene.raymarchMaxSteps = 100;
		render.scene.raymarchUnderstep = 0.9f;

		render.scene.ddaSpheresEnable = true;
		render.scene.ddaSpheresBoundSize = 2.0f;
		render.scene.ddaSpheresResolution = 50;

		// render.scene.maskedPlaneEnable = true;
		render.scene.maskedPlaneEnable = false;

		render.scene.skyDims = uvec2( 1024, 512 );
		render.scene.skyNeedsUpdate = true;
		render.scene.skyMode = 0;
		render.scene.skyConstantColor1 = vec3( 1.0f );
		render.scene.skyConstantColor2 = vec3( 0.0f );
		render.scene.skyInvert = false;
		render.scene.skyBrightnessScalar = 1.0f;
		render.scene.skyTime = 5.0f;

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

	// filter operation config
	bool filterEveryFrame;
	int filterSelector;

	bool showConfigWindow;	// tabbed interface for configuring scene/rendering parameters
	viewConfig_t view;		// parameters for ouput presentation
	renderConfig_t render;	// parameters for the tiled renderer

	// class holding the random number generators
	rngen_t rng;
};